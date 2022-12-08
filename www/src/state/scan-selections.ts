import { ScanId } from '../scan.types';

const SELECT_COLORS = ['#E69F00', '#93CEF1'] as const;
export type Color = typeof SELECT_COLORS[number];
export type ScanSelection = { id: ScanId; color: Color };
export type ScanSelections = Array<ScanSelection>;

export type ScanSelectionsPool = {
  selections: ScanSelections;
  freeColors: Array<Color>;
};

export const createSelectionPool = (): ScanSelectionsPool => ({
  selections: [],
  freeColors: [...SELECT_COLORS],
});

const findIndex = (id: ScanId, selections: ScanSelections) => {
  return selections.findIndex(({ id: scanId }) => id === scanId);
};

export const add = (
  id: ScanId,
  pool: ScanSelectionsPool
): ScanSelectionsPool => {
  const isFull = pool.selections.length >= SELECT_COLORS.length;
  const { selections, freeColors } = isFull
    ? remove(pool.selections[pool.selections.length - 1].id, pool)
    : pool;
  return {
    selections: [
      { id, color: freeColors[freeColors.length - 1] },
      ...selections,
    ],
    freeColors: freeColors.slice(0, freeColors.length - 1),
  };
};

export const remove = (
  id: ScanId,
  { selections, freeColors }: ScanSelectionsPool
): ScanSelectionsPool => {
  const idx = findIndex(id, selections);
  if (idx === -1) return { selections, freeColors };
  const { color: freeColor } = selections[idx];
  return {
    selections: [...selections.slice(0, idx), ...selections.slice(idx + 1)],
    freeColors: [freeColor, ...freeColors],
  };
};

export const has = (id: ScanId, selections: ScanSelections): boolean =>
  findIndex(id, selections) !== -1;

export const toggle = (
  id: ScanId,
  scans: ScanSelectionsPool
): ScanSelectionsPool =>
  has(id, scans.selections) ? remove(id, scans) : add(id, scans);

export const get = (
  id: ScanId,
  selections: ScanSelections
): ScanSelection | undefined => {
  const idx = findIndex(id, selections);
  return idx === -1 ? undefined : selections[idx];
};

export const compare = (a: ScanSelections, b: ScanSelections) =>
  a.length === b.length && a.every(({ id }) => findIndex(id, b) !== -1);
