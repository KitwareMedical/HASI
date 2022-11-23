import { LitElement, html } from "lit";
import { customElement } from "lit/decorators.js";

import "./biomarker-picker.js";

@customElement("plot-params")
export class PivotParams extends LitElement {
  render() {
    return html`
      <biomarker-picker parameter="Left Biomarker"></biomarker-picker>
      <biomarker-picker parameter="Bottom Biomarker"></biomarker-picker>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "plot-params": PivotParams;
  }
}
