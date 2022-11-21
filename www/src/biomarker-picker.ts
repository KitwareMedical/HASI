import { LitElement, html } from "lit";
import { customElement, property } from "lit/decorators.js";
import "@material/web/autocomplete/outlined-autocomplete.js";
import "@material/web/autocomplete/autocomplete-item.js";

@customElement("biomarker-picker")
export class BiomarkerPicker extends LitElement {
  @property() parameter: string = "";

  render() {
    return html`
      <md-outlined-autocomplete label=${this.parameter}>
        <md-autocomplete-item headline="age"></md-autocomplete-item>
        <md-autocomplete-item headline="weight"></md-autocomplete-item>
      </md-outlined-autocomplete>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    "biomarker-picker": BiomarkerPicker;
  }
}
