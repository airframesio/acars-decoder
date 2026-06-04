/**
 * @airframes/ads-runtime-ts
 *
 * The TypeScript runtime library for ADS-generated plugins. Provides the
 * DecoderPlugin base class, message/result types, the ResultFormatter, and
 * the shared utility modules previously hosted in acars-decoder-typescript/lib/.
 *
 * Generated plugins (output of `ads-gen --target ts`) import from this package
 * via the @airframes/ads-runtime-ts path alias configured in the consuming
 * repo's tsconfig.
 */

export { DecoderPlugin } from "./DecoderPlugin";
export type { MessageDecoder } from "./MessageDecoder";
export type {
  DecodeResult,
  DecoderPluginInterface,
  Message,
  Options,
  Qualifiers,
  RawFields,
} from "./DecoderPluginInterface";
export { ResultFormatter } from "./utils/result_formatter";
export { CoordinateUtils } from "./utils/coordinate_utils";
export { DateTimeUtils } from "./DateTimeUtils";
export { Arinc702Helper } from "./utils/arinc_702_helper";
export { FlightPlanUtils } from "./utils/flight_plan_utils";
export { RouteUtils } from "./utils/route_utils";
export { parseIcaoFpl } from "./utils/icao_fpl_utils";
export { MIAMCoreUtils } from "./utils/miam";
export { base64ToUint8Array, inflateData } from "./utils/compression";
export { ascii85Decode } from "./utils/ascii85";

export type { Route } from "./types/route";
export type { Waypoint } from "./types/waypoint";
export type { Wind } from "./types/wind";
