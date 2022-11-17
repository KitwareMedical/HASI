import { LitElement, css, html } from "lit";
import { customElement } from "lit/decorators.js";

/**
 * title
 *
 */
@customElement("processing-root")
export class ProcessingRoot extends LitElement {
  render() {
    return html`<top-app-bar title="Processing"> </top-app-bar> `;
  }
  static styles = css`
    :host {
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    "processing-root": ProcessingRoot;
  }
}
