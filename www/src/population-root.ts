import { LitElement, css, html } from 'lit';
import { customElement } from 'lit/decorators.js';

import './plot-params.js';
import './scan-table.js';
import './scan-views.js';

@customElement('population-root')
export class PopulationRoot extends LitElement {
  render() {
    return html`
      <h2>Population</h2>
      <div class="main-layout">
        <scan-table></scan-table>
        <plot-params></plot-params>
        <scan-views></scan-views>
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
    'population-root': PopulationRoot;
  }
}
