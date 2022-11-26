import { LitElement, html, css } from "lit";
import { customElement, property } from "lit/decorators.js";
import "@material/web/navigationdrawer/navigation-drawer.js";
import "@material/web/list/list.js";
import "@material/web/list/list-item.js";

import { PAGES } from "./pages";
import { Routes } from "@lit-labs/router";

/**
 * List of page links
 *
 */
@customElement("nav-menu")
export class NavMenu extends LitElement {
  @property() opened = true;
  @property() routes!: Routes;

  render() {
    return html`
      <md-navigation-drawer .opened=${this.opened}>
        <md-list role="menu">
          ${Object.values(PAGES).map(
            ({ path, title }) => html`
              <a href=${this.routes.link(path)}>
                <md-list-item headline=${title}> </md-list-item>
              </a>
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
