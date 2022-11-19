import { html } from "lit/static-html.js";
import { LitElement, css } from "lit";
import { customElement, property } from "lit/decorators.js";
import { Router } from "@lit-labs/router";

import "./top-app-bar.js";
import "./nav-menu.js";
import "./population-root.js";
import "./individual-root.js";
import "./processing-root.js";
import { PAGES } from "./pages.js";

const appTitle = "Osteoarthritis Biomarker Analysis";

/**
 * Hasi entry point
 *
 */
@customElement("hasi-app")
export class HasiApp extends LitElement {
  private _routes = new Router(
    this,
    Object.values(PAGES).map(({ path, tag }) => {
      return {
        path,
        render: () => html`<${tag}></${tag}>`,
      };
    })
  );

  @property() isMenuOpen = true;

  private _toggleMenuHandler() {
    this.isMenuOpen = !this.isMenuOpen;
  }

  render() {
    return html`
      <nav-menu .opened=${this.isMenuOpen}></nav-menu>
      <div class="center">
        <top-app-bar
          title=${appTitle}
          .isMenuOpen=${this.isMenuOpen}
          @toggleMenu="${this._toggleMenuHandler}"
        ></top-app-bar>
        <div class="main-content">${this._routes.outlet()}</div>
      </div>
    `;
  }

  static styles = css`
    :host {
      height: 100%;
      width: 100%;

      padding: 10px;
      display: flex;
    }

    .center {
      flex: 1;
      display: flex;
      flex-direction: column;
      overflow: auto;
    }

    .main-content {
      flex: 1;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    "hasi-app": HasiApp;
  }
}
