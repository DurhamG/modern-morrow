import { describe, expect, it } from "vitest";
import { cleanPhrase, MAX_PHRASE_LENGTH, phraseFromHeader, saveKey } from "../../src/phrase";

describe("cleanPhrase", () => {
  it("ignores case and spacing", () => {
    expect(cleanPhrase("  Silt   Strider\tBalmora \n")).toBe("silt strider balmora");
  });

  it("gives each accented letter one form", () => {
    // "é" typed as one character, and as "e" followed by a combining accent.
    expect(cleanPhrase("café")).toBe(cleanPhrase("café"));
  });

  it("lowers the case of other alphabets too", () => {
    expect(cleanPhrase("Силт Страйдер")).toBe("силт страйдер");
  });
});

describe("phraseFromHeader", () => {
  it("reads a phrase and cleans it up", () => {
    expect(phraseFromHeader("Phrase Silt%20%20Strider")).toBe("silt strider");
  });

  it("decodes UTF-8 sent percent-encoded", () => {
    expect(phraseFromHeader(`Phrase ${encodeURIComponent("Силт Страйдер")}`)).toBe("силт страйдер");
  });

  it("refuses a missing header, another scheme, and an empty phrase", () => {
    expect(phraseFromHeader(null)).toBeNull();
    expect(phraseFromHeader("Bearer abc")).toBeNull();
    expect(phraseFromHeader("Phrase ")).toBeNull();
    expect(phraseFromHeader("Phrase %20%20")).toBeNull();
  });

  it("refuses broken percent-encoding", () => {
    expect(phraseFromHeader("Phrase %E0%A4%A")).toBeNull();
  });

  it("allows phrases up to the limit", () => {
    expect(phraseFromHeader(`Phrase ${"a".repeat(MAX_PHRASE_LENGTH)}`)).toHaveLength(MAX_PHRASE_LENGTH);
    expect(phraseFromHeader(`Phrase ${"a".repeat(MAX_PHRASE_LENGTH + 1)}`)).toBeNull();
  });
});

describe("saveKey", () => {
  it("is the SHA-256 of the phrase in hex", async () => {
    // The SHA-256 of "hello world", from any SHA-256 tool, such as sha256sum.
    expect(await saveKey("hello world")).toBe("b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
  });

  it("hashes the phrase's UTF-8", async () => {
    // The SHA-256 of the bytes C3 A9, which are "é" in UTF-8.
    expect(await saveKey("é")).toBe("4a99557e4033c3539de2eb65472017cad5f9557f7a0625a09f1c3f6e2ba69c4c");
  });
});
