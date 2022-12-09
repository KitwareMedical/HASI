import { LitElement, html, css } from 'lit';
import { customElement, property } from 'lit/decorators.js';

import './biomarker-picker.js';

@customElement('biomarker-chart')
export class BiomarkerChart extends LitElement {
  @property() imageSource: string = '';

  render() {
    return html`
      <div>
        <img src=${this.imageSource} class="chart" />
      </div>
      <div>
        <biomarker-picker parameter="Biomarker bottom"></biomarker-picker>
      </div>
    `;
  }

  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      justify-content: center;
      align-items: center;
    }

    .chart {
      object-fit: contain;
      width: 100%;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'biomarker-chart': BiomarkerChart;
  }
}
