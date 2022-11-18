import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";
import "@material/web/navigationdrawer/navigation-drawer.js";
import "@material/web/button/filled-link-button.js";
import "@material/web/list/list.js";
import "@material/web/list/list-item.js";

import { PAGES } from "./pages";

/**
 * List of page links
 *
 */
@customElement("nav-menu")
export class NavMenu extends LitElement {
  @property() opened = true;

  render() {
    return html`
      <md-navigation-drawer .opened=${this.opened}>
        <md-list role="menu">
          ${Object.values(PAGES).map(
            ({ path, title }) => html`
              <md-list-item>
                <md-filled-link-button
                  label="${title}"
                  href="${path}"
                  slot="start"
                ></md-filled-link-button>
              </md-list-item>
            `
          )}
        </md-list>
      </md-navigation-drawer>
    `;
  }

  static styles = css`
    :host {
      overflow: auto;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    "nav-menu": NavMenu;
  }
}
