import { LitElement, css, html } from "lit";
import { customElement } from "lit/decorators.js";
import { Router } from "@lit-labs/router";
import "@material/web/navigationdrawer/navigation-drawer.js";
import "@material/web/button/filled-link-button.js";

import "./top-app-bar.js";

const appTitle = "Osteoarthritis Biomarker Analysis";

/**
 * Hasi entry point
 *
 */
@customElement("hasi-app")
export class HasiApp extends LitElement {
  private _routes = new Router(this, [
    { path: "/", render: () => html`<h1>Population</h1>` },
    { path: "/individual", render: () => html`<h1>Individual</h1>` },
    { path: "/processing", render: () => html`<h1>Processing</h1>` },
  ]);

  render() {
    return html`
      <md-navigation-drawer opened="true">
        <h3>${appTitle}</h3>
        <md-filled-link-button
          label="Population"
          href="/"
        ></md-filled-link-button>
        <md-filled-link-button
          label="Individual"
          href="individual"
        ></md-filled-link-button>
        <md-filled-link-button
          label="Processing"
          href="processing"
        ></md-filled-link-button>
      </md-navigation-drawer>
      <div>
        <top-app-bar>
          <h1>${this._routes.outlet()}</h1>
        </top-app-bar>
        <div class="main-content">
          <p>
            Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do
            eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
            ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
            aliquip ex ea commodo consequat. Duis aute irure dolor in
            reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
            pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
            culpa qui officia deserunt mollit anim id est laborum.
          </p>
          <p>
            Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do
            eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
            ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
            aliquip ex ea commodo consequat. Duis aute irure dolor in
            reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
            pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
            culpa qui officia deserunt mollit anim id est laborum.
          </p>
        </div>
      </div>
    `;
  }
  static styles = css`
    :host {
      display: flex;
    }

    .main-content {
      max-width: 1280px;
      margin: 0 auto;
      padding: 2rem;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    "hasi-app": HasiApp;
  }
}
