import { ScanId } from '../scan.types';

const SELECT_COLORS = ['Aqua ', 'Aquamarine'] as const;
export type Color = typeof SELECT_COLORS[number];
export type SelectedScan = { id: ScanId; color: Color };

export type ScanSelection = {
  selected: Array<SelectedScan>;
  freeColors: Array<Color>;
};

export const createSelectedScans = (): ScanSelection => ({
  selected: [],
  freeColors: [...SELECT_COLORS],
});

const findIndex = (id: ScanId, selected: ScanSelection['selected']) => {
  return selected.findIndex(({ id: scanId }) => id === scanId);
};

export const add = (id: ScanId, selection: ScanSelection): ScanSelection => {
  const isFull = selection.selected.length >= SELECT_COLORS.length;
  const { selected, freeColors } = isFull
    ? remove(selection.selected[selection.selected.length - 1].id, selection)
    : selection;
  return {
    selected: [{ id, color: freeColors[freeColors.length - 1] }, ...selected],
    freeColors: freeColors.slice(0, freeColors.length - 1),
  };
};

export const remove = (
  id: ScanId,
  { selected, freeColors }: ScanSelection
): ScanSelection => {
  const idx = findIndex(id, selected);
  if (idx === -1) return { selected, freeColors };
  const { color: freeColor } = selected[idx];
  return {
    selected: [...selected.slice(0, idx), ...selected.slice(idx + 1)],
    freeColors: [freeColor, ...freeColors],
  };
};

export const has = (id: ScanId, { selected }: ScanSelection): boolean =>
  findIndex(id, selected) !== -1;

export const toggle = (id: ScanId, scans: ScanSelection): ScanSelection =>
  has(id, scans) ? remove(id, scans) : add(id, scans);

export const get = (
  id: ScanId,
  { selected }: ScanSelection
): SelectedScan | undefined => {
  const idx = findIndex(id, selected);
  return idx === -1 ? undefined : selected[idx];
};
