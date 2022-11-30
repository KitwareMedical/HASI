import { LitElement, css, html } from "lit";
import { customElement, property } from "lit/decorators.js";
import { repeat } from "lit/directives/repeat.js";

import { connectState } from "./utils/SelectState.js";
import "./feature-bar.js";
import "./scan-view.js";
import { Feature, FEATURE_KEYS } from "./scan.types.js";

@customElement("feature-scans")
export class FeatureScans extends LitElement {
  @property() feature: Feature = FEATURE_KEYS[0];

  scanIds = connectState(
    this,
    (state) => [...state.context.selectedScans],
    (oldScans, newScans) =>
      oldScans.length === newScans.length &&
      oldScans.every((id) => newScans.includes(id))
  );

  render() {
    return html`
      <feature-bar .feature=${this.feature}></feature-bar>
      <div class="scans">
        ${repeat(
          this.scanIds.selected() ?? [],
          (scanId) => scanId,
          (scanId) =>
            html`<scan-view
              .scanId=${scanId}
              .feature=${this.feature}
            ></scan-view>`
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
      flex: 1;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    "feature-scans": FeatureScans;
  }
}
