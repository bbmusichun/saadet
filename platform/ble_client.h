/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Included by the checked fm1-base MIDI overlay, after the upstream central's
 * state declarations. A discovered characteristic is not a working MIDI link:
 * read it, then wait for the peer to acknowledge the notification subscription.
 * Keep those operations out of the profile-search callback (its GATT query may
 * still own the client). Pair only on an explicit ATT security error. */
#include "btstack/le/gatt.h"

enum {
    SD_MC_IDLE, SD_MC_DISCOVER, SD_MC_READ, SD_MC_READING,
    SD_MC_SUBSCRIBE, SD_MC_SUBSCRIBING, SD_MC_PAIR, SD_MC_PAIRING, SD_MC_SECURITY
};
static u8 sd_mc_phase, sd_mc_paired, sd_mc_encrypted, sd_mc_security_retry, sd_mc_queue_error;
static u16 sd_mc_ccc, sd_mc_end;
static u32 sd_mc_wait_at;
static int32_t sd_mc_code = -1;

int32_t ble_midi_pairing_code(void)
{
    return mc_handle && mc_state == BLE_MC_SEARCHING ? sd_mc_code : -1;
}

static void sd_mc_passkey(const u8 *packet, u16 size, int display)
{
    if (size < (display ? 15u : 4u) || !mc_handle ||
        little_endian_read_16(packet, 2) != mc_handle ||
        mc_state != BLE_MC_SEARCHING || sd_mc_phase == SD_MC_IDLE) return;
    if (!display) { sd_mc_code = -1; return; }
    /* SDK SM_EVENT_PASSKEY_DISPLAY_NUMBER carries a little-endian uint32 at
     * byte 11; use its generated code, never a fixed PIN. Do not log the PIN. */
    u32 code = (u32)packet[11] | (u32)packet[12] << 8 |
               (u32)packet[13] << 16 | (u32)packet[14] << 24;
    if (code > 999999u) return;
    sd_mc_code = (int32_t)code;
    sd_mc_paired = 1;
    mc_t0 = shim_ms();
    printf("MCPIN enter code on peer\n");
}

static void sd_mc_reset(void)
{
    sd_mc_phase = SD_MC_IDLE;
    sd_mc_code = -1;
    sd_mc_paired = 0;
    sd_mc_encrypted = sd_mc_security_retry = sd_mc_queue_error = 0;
    sd_mc_ccc = sd_mc_end = 0;
}

static void sd_mc_fail(const char *why, unsigned error)
{
    /* The base folds consecutive lines sharing their first five characters
     * and keeps only 26 characters when folding: put the error code first. */
    printf("MCERR %x %s\n", error, why);
    sd_mc_code = -1;
    sd_mc_phase = SD_MC_IDLE;
    if (mc_handle) ble_op_disconnect(mc_handle);
}

static void sd_mc_search(search_result_t *r)
{
    if (!mc_handle || mc_state != BLE_MC_SEARCHING || sd_mc_phase != SD_MC_DISCOVER) return;
    if (r == (void *)-1) {
        if (!mc_value || !sd_mc_ccc) {
            sd_mc_fail("missing MIDI/CCC", 0);
            return;
        }
        sd_mc_phase = SD_MC_READ;
        mc_t0 = shim_ms();
        printf("MCFND value %x CCC %x\n", mc_value, sd_mc_ccc);
        return;
    }
    if (!r->services.uuid16 && uuid_is(r->services.uuid128, midi_svc_le) &&
        !r->characteristic.uuid16 && uuid_is(r->characteristic.uuid128, midi_chr_le)) {
        mc_value = r->characteristic.value_handle;
        sd_mc_end = r->characteristic.end_handle;
        sd_mc_ccc = 0;
    }
}

static void sd_mc_descriptor(charact_descriptor_t *d)
{
    if (sd_mc_phase == SD_MC_DISCOVER && mc_value && d &&
        d->uuid16 == 0x2902 && d->handle > mc_value && d->handle <= sd_mc_end)
        sd_mc_ccc = d->handle;
}

static void sd_mc_att(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    if (packet_type != HCI_EVENT_PACKET || size < 5 || packet[0] != GATT_EVENT_QUERY_COMPLETE ||
        !mc_handle || little_endian_read_16(packet, 2) != mc_handle || mc_state != BLE_MC_SEARCHING ||
        (sd_mc_phase != SD_MC_READING && sd_mc_phase != SD_MC_SUBSCRIBING)) return;
    u8 status = packet[4];
    if (status) {
        /* Insufficient authentication, encryption key size, or encryption. */
        if (status == 0x05 || status == 0x0c || status == 0x0f) {
            if (!sd_mc_encrypted && sd_mc_code >= 0) {
                sd_mc_phase = SD_MC_PAIRING;
                mc_t0 = shim_ms();
                return;
            }
            if (sd_mc_encrypted && sd_mc_security_retry < 5) {
                /* iOS can still reject ATT until key distribution/bonding
                 * finishes after the encryption event. Resume on the SDK's
                 * bond-complete event, or retry after a short settling time
                 * for peers that do not store a bond. Never request another
                 * pairing or retry an authentication rejection indefinitely. */
                sd_mc_security_retry++;
                printf("MCSEC auth %x retry %u\n", status, sd_mc_security_retry);
                sd_mc_phase = SD_MC_SECURITY;
                sd_mc_wait_at = mc_t0 = shim_ms();
                return;
            }
            if (!sd_mc_paired && !sd_mc_encrypted) {
                printf("MCPAR required %x\n", status);
                sd_mc_paired = 1;
                sd_mc_phase = SD_MC_PAIR;
                mc_t0 = shim_ms();
                return;
            }
        }
        /* The initial read is a handshake attempt, not the subscription.
         * Android's BluetoothMidiDevice also proceeds to the CCC after its
         * read callback regardless of status. Some peers reject the read
         * after encryption yet permit notifications. Try the real CCC once
         * after bounded security settling, or if reading is unsupported.
         * Only an acknowledged CCC write can put this link in READY. */
        if (sd_mc_phase == SD_MC_READING &&
            (status == 0x02 || status == 0x06 ||
             (sd_mc_encrypted && (status == 0x05 || status == 0x0c || status == 0x0f)))) {
            printf("MCRSK read %x, try CCC\n", status);
            sd_mc_phase = SD_MC_SUBSCRIBE;
            mc_t0 = shim_ms();
            return;
        }
        sd_mc_fail("MIDI setup failed", status);
        return;
    }
    if (sd_mc_phase == SD_MC_READING) {
        sd_mc_phase = SD_MC_SUBSCRIBE;
        mc_t0 = shim_ms();
    } else {
        sd_mc_phase = SD_MC_IDLE;
        mc_state = BLE_MC_READY;
        printf("MCOK MIDI subscription confirmed\n");
        bt_ble_adv_enable(1);
    }
}

static void sd_mc_encryption(const u8 *packet, u16 size)
{
    if (size < 6 || !mc_handle || little_endian_read_16(packet, 3) != mc_handle ||
        mc_state != BLE_MC_SEARCHING || sd_mc_phase == SD_MC_IDLE) return;
    printf("MCENC phase %u on %u\n", sd_mc_phase, packet[5]);
    if (packet[2] || !packet[5]) {
        sd_mc_fail("encryption failed", packet[2]);
        return;
    }
    sd_mc_code = -1;
    sd_mc_encrypted = 1;
    mc_t0 = shim_ms();
    if (sd_mc_phase == SD_MC_PAIR || sd_mc_phase == SD_MC_PAIRING) {
        printf("MCRST encrypted, retry MIDI\n");
        sd_mc_phase = SD_MC_READ;
    }
}

static void sd_mc_bond_event(u16 handle, u8 event)
{
    if (event != 0x10 || !mc_handle || handle != mc_handle || !sd_mc_encrypted ||
        mc_state != BLE_MC_SEARCHING || sd_mc_phase != SD_MC_SECURITY) return;
    /* SM_EVENT_PAIR_SUB_ADD_LIST_SUCCESS: the SDK has received/stored keys. */
    printf("MCBOK bond stored, retry MIDI\n");
    sd_mc_phase = SD_MC_READ;
    mc_t0 = shim_ms();
}

static void sd_mc_poll(void)
{
    static u8 on[2] = {1, 0};
    if (!mc_handle || mc_state != BLE_MC_SEARCHING) return;
    if (sd_mc_phase == SD_MC_SECURITY) {
        if (shim_ms() - sd_mc_wait_at < 200u) return;
        sd_mc_phase = SD_MC_READ;
    }
    u8 phase = sd_mc_phase, result;
    if (phase == SD_MC_PAIR) {
        sd_mc_phase = SD_MC_PAIRING;
        mc_t0 = shim_ms();
        if (!sm_api_request_pairing(mc_handle)) sd_mc_fail("cannot start pairing", 0);
        return;
    }
    if (phase != SD_MC_READ && phase != SD_MC_SUBSCRIBE) return;
    sd_mc_phase = phase == SD_MC_READ ? SD_MC_READING : SD_MC_SUBSCRIBING;
    if (phase == SD_MC_READ)
        result = gatt_client_read_value_of_characteristic_using_value_handle(sd_mc_att, mc_handle, mc_value);
    else
        result = gatt_client_write_value_of_characteristic(sd_mc_att, mc_handle, sd_mc_ccc, sizeof on, on);
    /* Busy is not a completed request. Retry from the main loop, bounded by
     * the existing setup timeout; no unacknowledged operation can say READY. */
    if (result) {
        sd_mc_phase = phase;
        if (sd_mc_queue_error != result) printf("MCBSY %x phase %u\n", result, phase);
        sd_mc_queue_error = result;
    } else {
        sd_mc_queue_error = 0;
        printf(phase == SD_MC_READ ? "MCRDQ handle %x\n" : "MCSBQ handle %x\n",
               phase == SD_MC_READ ? mc_value : sd_mc_ccc);
    }
}

static u32 sd_mc_timeout(void)
{
    return sd_mc_code >= 0 || sd_mc_phase == SD_MC_PAIR || sd_mc_phase == SD_MC_PAIRING ? 30000u : 5000u;
}
