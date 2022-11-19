let plotParameters = {
  leftBiomarker: "",
  bottomBiomarker: "",
};

export type PlotParameter = keyof typeof plotParameters;

export const setParameter = (parameter: PlotParameter, value: string) => {
  plotParameters = { ...plotParameters, [parameter]: value };
};
