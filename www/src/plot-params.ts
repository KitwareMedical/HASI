import { LitElement, html } from "lit";
import { customElement } from "lit/decorators.js";

import { setParameter, PlotParameter } from "./plot.store.js";
import "./biomarker-picker.js";

@customElement("plot-params")
export class PivotParams extends LitElement {
  private _setPlotParameter =
    (parameter: PlotParameter) => (e: CustomEvent) => {
      setParameter(parameter, e.detail.value);
    };

  render() {
    return html`
      <biomarker-picker
        parameter="Left Biomarker"
        @autocomplete-value-changed=${this._setPlotParameter("leftBiomarker")}
      ></biomarker-picker>
      <biomarker-picker
        parameter="Bottom Biomarker"
        @autocomplete-value-changed=${this._setPlotParameter("bottomBiomarker")}
      ></biomarker-picker>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "plot-params": PivotParams;
  }
}
