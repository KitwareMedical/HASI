import { LitElement, css, html } from "lit";
import { customElement } from "lit/decorators.js";

@customElement("individual-root")
export class IndividualRoot extends LitElement {
  render() {
    return html` <h2>Individual</h2> `;
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
