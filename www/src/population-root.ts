import { LitElement, css, html } from "lit";
import { customElement } from "lit/decorators.js";

import "./plot-params";

@customElement("population-root")
export class PopulationRoot extends LitElement {
  render() {
    return html`
      <h2>Population</h2>
      <div class="main-layout">
        <plot-params> </plot-params>
        <div>Image Viewer</div>
      </div>
    `;
  }

  static styles = css`
    :host {
      height: 100%;
      display: flex;
      flex-direction: column;
    }

    .main-layout {
      flex: 1;
      display: flex;
      flex-direction: column;
    }

    .main-layout > * {
      flex: 1;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    "population-root": PopulationRoot;
  }
}
