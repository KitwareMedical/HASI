import { html, literal } from "lit/static-html.js";
import { LitElement, css } from "lit";
import { customElement } from "lit/decorators.js";
import { Router } from "@lit-labs/router";
import "@material/web/navigationdrawer/navigation-drawer.js";
import "@material/web/button/filled-link-button.js";
import "@material/web/list/list.js";
import "@material/web/list/list-item.js";

import "./top-app-bar.js";
import "./population-root.js";
import "./individual-root.js";
import "./processing-root.js";

const PAGES = {
  population: { title: "Population", path: "/", tag: literal`population-root` },
  individual: {
    title: "Individual",
    path: "/individual",
    tag: literal`individual-root`,
  },
  processing: {
    title: "Processing",
    path: "/processing",
    tag: literal`processing-root`,
  },
};

const PAGE_ITEMS = Object.values(PAGES).map(
  ({ path, title }) => html`
    <md-list-item>
      <md-filled-link-button
        label="${title}"
        href="${path}"
        slot="start"
      ></md-filled-link-button>
    </md-list-item>
  `
);

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

  render() {
    return html`
      <md-navigation-drawer opened="true">
        <h3>${appTitle}</h3>
        <md-list role="menu"> ${PAGE_ITEMS} </md-list>
      </md-navigation-drawer>
      <div class="main-content">${this._routes.outlet()}</div>
    `;
  }
  static styles = css`
    :host {
      height: 100%;
      width: 100%;

      padding: 10px;
      display: flex;
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
