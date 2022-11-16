import { LitElement, css, html } from "lit";
import { customElement, property } from "lit/decorators.js";

/**
 * title
 *
 */
@customElement("top-app-bar")
export class TopAppBar extends LitElement {
  @property() title: string = "title";
  render() {
    return html`<h1>${this.title}</h1>`;
  }
  static styles = css`
    :host {
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    "top-app-bar": TopAppBar;
  }
}
