import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";
import { map } from "lit/directives/map.js";
import "@material/web/autocomplete/outlined-autocomplete.js";
import "@material/web/autocomplete/autocomplete-item.js";
import "@material/web/iconbutton/standard-icon-button";

import { Feature, FEATURES } from "./scan.types.js";

@customElement("feature-bar")
export class FeatureBar extends LitElement {
  @property() feature!: Feature;

  clickHandler() {
    const event = new Event("feature-close", { bubbles: true, composed: true });
    this.dispatchEvent(event);
  }

  render() {
    return html`
      <md-outlined-autocomplete
        label="Select Feature"
        .value=${FEATURES[this.feature].name}
      >
        ${map(
          Object.values(FEATURES),
          ({ name, long }) =>
            html`<md-autocomplete-item
              headline="${name}"
              supportingText="${long}"
            ></md-autocomplete-item>`
        )}
      </md-outlined-autocomplete>

      <md-standard-icon-button @click="${this.clickHandler}" icon="close">
      </md-standard-icon-button>
    `;
  }

  static styles = css`
    :host {
      position: relative;
      display: flex;
      justify-content: center;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    "feature-bar": FeatureBar;
  }
}
