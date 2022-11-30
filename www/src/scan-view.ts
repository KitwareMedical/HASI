import { LitElement, css, html } from "lit";
import { customElement, property } from "lit/decorators.js";
import { Feature, ScanId } from "./scan.types";

@customElement("scan-view")
export class ScanView extends LitElement {
  @property() scanId!: ScanId;
  @property() feature!: Feature;

  render() {
    return html`scanId: ${this.scanId}, feature: ${this.feature}`;
  }

  static styles = css`
    :host {
      text-align: center;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    "scan-view": ScanView;
  }
}
