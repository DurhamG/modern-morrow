// The phrase that names a player's cloud save, and the name of the save in the bucket.

export const MAX_PHRASE_LENGTH = 200;

// Makes the phrases a player would see as the same into one: the same letters in any case, and any spacing. NFC
// gives each accented letter one form, since keyboards on different devices can type them differently.
export function cleanPhrase(phrase: string): string {
  return phrase.normalize("NFC").trim().replace(/\s+/gu, " ").toLowerCase();
}

// Reads the phrase from an "Authorization: Phrase <phrase>" header. Headers only carry ASCII safely, so the game
// sends the phrase as UTF-8 with each other byte percent-encoded, the way a URL carries it. Returns the cleaned-up
// phrase, or null when there is none or it's too long.
export function phraseFromHeader(header: string | null): string | null {
  const match = header?.match(/^Phrase (.*)$/s);
  if (!match) return null;
  let phrase: string;
  try {
    phrase = cleanPhrase(decodeURIComponent(match[1]));
  } catch {
    return null;
  }
  if (phrase.length === 0 || phrase.length > MAX_PHRASE_LENGTH) return null;
  return phrase;
}

// The save's name in the bucket: the SHA-256 of the cleaned-up phrase's UTF-8, in hex. So the phrase a player typed
// doesn't show in the dashboard.
export async function saveKey(cleanedPhrase: string): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(cleanedPhrase));
  return Array.from(new Uint8Array(digest), (b) => b.toString(16).padStart(2, "0")).join("");
}
