import { LitElement, css, html } from "lit";
import { customElement } from "lit/decorators.js";

/**
 * title
 *
 */
@customElement("top-app-bar")
export class TopAppBar extends LitElement {
  render() {
    return html`
      <div>
        <slot></slot>
      </div>
    `;
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
