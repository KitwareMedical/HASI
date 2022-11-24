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
