// The details of a stored save, which GET /v1/save/info returns so the game can show the save without downloading it.
import type { SaveInfo } from "./savecheck";

export interface SaveDetails {
  playerName: string;
  playerLevel?: number;
  cell?: string;
  description?: string;
  contentFiles: string[];
  // True when the list above was cut short to fit.
  moreContentFiles?: boolean;
  formatVersion: number;
  size: number;
  uploaded: string;
}

// R2 keeps up to 8 KB of custom metadata with each object. The details are kept there as JSON, so reading them
// doesn't read the save. These limits keep them well inside it. The longest name, cell or description the game
// writes is far shorter, but a load order can have hundreds of content files.
export const MAX_DETAILS_BYTES = 6000;
export const MAX_TEXT_LENGTH = 256;

const utf8Length = (text: string) => new TextEncoder().encode(text).length;

const cut = (text: string | undefined) => text?.slice(0, MAX_TEXT_LENGTH);

export function saveDetails(info: SaveInfo, size: number, uploaded: Date): SaveDetails {
  const details: SaveDetails = {
    playerName: cut(info.playerName) ?? "",
    playerLevel: info.playerLevel,
    cell: cut(info.cell),
    description: cut(info.description),
    contentFiles: info.contentFiles.map((name) => name.slice(0, MAX_TEXT_LENGTH)),
    formatVersion: info.formatVersion,
    size,
    uploaded: uploaded.toISOString(),
  };
  // Keeps the first content files, which the game loads first, and drops the rest until the details fit.
  while (utf8Length(JSON.stringify(details)) > MAX_DETAILS_BYTES) {
    details.contentFiles.pop();
    details.moreContentFiles = true;
  }
  return details;
}
