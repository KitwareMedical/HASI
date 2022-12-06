import { LitElement, css, html, PropertyValueMap } from 'lit';
import { customElement } from 'lit/decorators.js';
import '@shoelace-style/shoelace/dist/components/details/details.js';
import { setDefaultAnimation } from '@shoelace-style/shoelace/dist/utilities/animation-registry.js';

import './plot-params.js';
import './scan-table.js';
import './scan-views.js';

setDefaultAnimation('details.show', {
  keyframes: [{ opacity: '0' }, { opacity: '1' }],
  options: {
    duration: 200,
  },
});
setDefaultAnimation('details.hide', null);

@customElement('population-root')
export class PopulationRoot extends LitElement {
  protected firstUpdated(
    _changedProperties: PropertyValueMap<any> | Map<PropertyKey, unknown>
  ): void {
    this.renderRoot.querySelectorAll('sl-details').forEach((details) => {
      var sheet = new CSSStyleSheet();
      sheet.replaceSync(`.details__body { flex: 1;
       display: flex;
       flex-direction: column;
      }`);
      details!.shadowRoot!.adoptedStyleSheets = [
        ...details!.shadowRoot!.adoptedStyleSheets,
        sheet,
      ];
    });
  }

  render() {
    return html`
      <h2 class="title">Population</h2>
      <div class="main-layout">
        <sl-details summary="Scans" open>
          <scan-table></scan-table>
        </sl-details>

        <sl-details summary="Biomarkers" open>
          <plot-params></plot-params>
        </sl-details>

        <sl-details summary="Features" open>
          <scan-views></scan-views>
        </sl-details>
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
      display: flex;
      flex-direction: column;
    }

    .main-layout > *::part(base) {
      flex: 1;
      display: flex;
      flex-direction: column;
    }

    .main-layout > *::part(content) {
      flex: 1;
      display: flex;
      flex-direction: column;
    }

    .main-layout > * > * {
      flex: 1;
      display: flex;
      flex-direction: column;
    }

    sl-details:not([open]) {
      flex: 0;
    }

    sl-details::part(header) {
      background-color: var(--primarycolor);
      color: var(--textwhite);
      font-size: 22px;
      margin: 0.4rem 0;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'population-root': PopulationRoot;
  }
}
