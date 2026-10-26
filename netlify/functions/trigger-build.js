// netlify/functions/trigger-build.js
exports.handler = async (event, context) => {
  // Csak POST kérést fogadunk el
  if (event.httpMethod !== "POST") {
    return { statusCode: 405, body: "Method Not Allowed" };
  }

  // A GitHub tokent és a repó adatait a Netlify környezeti változóiból olvassuk
  const GITHUB_TOKEN = process.env.GITHUB_TOKEN;
  const REPO_OWNER = process.env.GITHUB_REPO_OWNER;
  const REPO_NAME = process.env.GITHUB_REPO_NAME;

  try {
    // Meghívjuk a GitHub API-t, hogy indítson egy workflow-t (vagyis a build.yml-t)
    const response = await fetch(`https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/actions/workflows/build.yml/dispatches`, {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${GITHUB_TOKEN}`,
        'Accept': 'application/vnd.github.v3+json',
        'User-Agent': 'Netlify-Function'
      },
      body: JSON.stringify({
        ref: 'main' // A branch neve
      })
    });

    if (response.ok) {
      return {
        statusCode: 200,
        body: JSON.stringify({ message: "Sikeres indítás! A GitHub épp dolgozik a firmware-en." })
      };
    } else {
      const errorData = await response.text();
      return {
        statusCode: response.status,
        body: JSON.stringify({ message: "Hiba a GitHub hívásakor", error: errorData })
      };
    }
  } catch (error) {
    return {
      statusCode: 500,
      body: JSON.stringify({ message: "Szerver hiba", error: error.message })
    };
  }
};
