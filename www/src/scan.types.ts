export type ScanId = string;

export interface Scan {
  id: ScanId;
  age: number;
  weight: number;
  sex: "Male" | "Female";
}

export type Field = keyof Scan;

// Make runtime array of fields
// Record type ensures, we have no double or missing keys, values can be neglected
function createKeys(keyRecord: Record<keyof Scan, any>): (keyof Scan)[] {
  return Object.keys(keyRecord) as any;
}

export const fields = createKeys({
  id: 1,
  age: 1,
  weight: 1,
  sex: 1,
});

export const FEATURES = {
  rawMri: {
    name: "Machine Output",
    long: "Raw image from the MRI machine",
  },
  cartilageThickness: {
    name: "Cartilage Thickness",
    long: "3D model of the cartilage color mapped with local thickness",
  },
  volumeCartilage: {
    name: "Volume Cartilage",
    long: "Segmented cartilage in the MRI",
  },
} as const;

export type Feature = keyof typeof FEATURES;

export const FEATURE_KEYS = Object.keys(FEATURES) as Feature[];
export const NAME_TO_KEY: Record<string, Feature> = Object.entries(
  FEATURES
).reduce((nameToKeys, [key, { name }]) => ({ ...nameToKeys, [name]: key }), {});
