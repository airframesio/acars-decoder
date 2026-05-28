import type { DecodeResult, Message, Options } from "./DecoderPluginInterface";

/**
 * Minimal contract for a MessageDecoder — the dispatcher that runs registered
 * plugins against a message. The concrete dispatcher lives in the consuming
 * language repo (acars-decoder-typescript/lib/MessageDecoder.ts) and implements
 * this interface; the runtime package only needs the contract so DecoderPlugin
 * can reference it (used by wrapper plugins like CBand that recurse).
 */
export interface MessageDecoder {
  decode(message: Message, options?: Options): DecodeResult;
}
