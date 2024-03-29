import { LitElement, css, html } from 'lit';
import { customElement, property } from 'lit/decorators.js';
import { repeat } from 'lit/directives/repeat.js';

import { connectState } from './utils/select-state.js';
import './feature-bar.js';
import './scan-view.js';
import { Feature, FEATURE_KEYS } from './scan.types.js';
import { compare } from './state/scan-selections.js';

@customElement('feature-scans')
export class FeatureScans extends LitElement {
  @property() feature: Feature = FEATURE_KEYS[0];

  scans = connectState(
    this,
    (state) => state.context.scanSelectionsPool.selections,
    compare
  );

  render() {
    return html`
      <feature-bar .feature=${this.feature}></feature-bar>
      <div class="scans">
        ${repeat(
          this.scans.value ?? [],
          ({ id }) => id,
          (scan) =>
            html`<scan-view .scan=${scan} .feature=${this.feature}></scan-view>`
        )}
      </div>
    `;
  }

  static styles = css`
    :host {
      margin-top: 0.4rem;
      display: flex;
      flex-direction: column;
    }

    .scans {
      flex: 1;
      display: flex;
      flex-direction: column;
    }

    .scans > * {
      margin: 0.2rem;
      flex: 1;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'feature-scans': FeatureScans;
  }
}
