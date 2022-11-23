export interface Scan {
  id: string;
  age: number;
  weight: number;
  sex: "Male" | "Female";
}

export type Field = keyof Scan;

// Record type ensures, we have no double or missing keys, values can be neglected
function createKeys(keyRecord: Record<keyof Scan, any>): (keyof Scan)[] {
  return Object.keys(keyRecord) as any;
}

// runtime array of fields
export const fields = createKeys({
  id: 1,
  age: 1,
  weight: 1,
  sex: 1,
});
