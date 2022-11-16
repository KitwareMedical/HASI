import { LitElement, css, html } from "lit";
import { customElement } from "lit/decorators.js";

/**
 * title
 *
 */
@customElement("individual-root")
export class IndividualRoot extends LitElement {
  render() {
    return html`<top-app-bar title="Individual"> </top-app-bar> `;
  }
  static styles = css`
    :host {
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    "individual-root": IndividualRoot;
  }
}
