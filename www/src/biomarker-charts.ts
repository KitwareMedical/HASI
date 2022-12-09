import { LitElement, html, css } from 'lit';
import { customElement } from 'lit/decorators.js';
import scatterPlot from './assets/scatter-plot.png';
import darkBar from './assets/dark-bar.png';
import lightBar from './assets/light-bar.png';

import './biomarker-chart.js';

@customElement('biomarker-charts')
export class BiomarkerCharts extends LitElement {
  render() {
    return html`
      <biomarker-chart imageSource=${scatterPlot}></biomarker-chart>
      <biomarker-chart imageSource=${darkBar}></biomarker-chart>
      <biomarker-chart imageSource=${lightBar}></biomarker-chart>
    `;
  }

  static styles = css`
    :host {
      display: grid;
      grid-template-columns: 1.5fr 1fr 1fr;
      grid-auto-flow: column;
      gap: 0.4rem;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'biomarker-charts': BiomarkerCharts;
  }
}
