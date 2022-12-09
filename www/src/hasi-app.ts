import { ContextProvider } from '@lit-labs/context';
import { html } from 'lit/static-html.js';
import { LitElement, css } from 'lit';
import { customElement, property } from 'lit/decorators.js';
import { Router } from '@lit-labs/router';
// @ts-ignore: Property 'UrlPattern' does not exist
if (!globalThis.URLPattern) {
  import('urlpattern-polyfill');
}
import '@shoelace-style/shoelace/dist/themes/light.css';

import { createService, hasiContext, saveState } from './state/hasi.machine';

import './top-app-bar.js';
import './nav-menu.js';
import './population-root.js';
import './individual-root.js';
import './processing-root.js';
import { PAGES } from './pages.js';

const APP_TITLE = 'Osteoarthritis Biomarkers' as const;

/**
 * Hasi entry point
 *
 */
@customElement('hasi-app')
export class HasiApp extends LitElement {
  // @ts-ignore
  private provider = new ContextProvider(this, hasiContext, {
    service: createService(),
  });

  private routes = new Router(
    this,
    Object.values(PAGES).map(({ path, tag }) => {
      return {
        path,
        render: () => {
          return html`<${tag}></${tag}>`;
        },
        enter: () => {
          saveState(this.provider.value.service.getSnapshot());
          return true;
        },
      };
    })
  );

  @property() isMenuOpen = true;

  private _toggleMenuHandler() {
    this.isMenuOpen = !this.isMenuOpen;
  }

  render() {
    return html`
      <nav-menu .opened=${this.isMenuOpen} .routes=${this.routes}></nav-menu>
      <div class="center">
        <top-app-bar
          title=${APP_TITLE}
          .isMenuOpen=${this.isMenuOpen}
          @toggleMenu="${this._toggleMenuHandler}"
        ></top-app-bar>
        <div class="main-content">${this.routes.outlet()}</div>
      </div>
    `;
  }

  static styles = css`
    :host {
      height: 100%;
      width: 100%;

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
    'hasi-app': HasiApp;
  }
}
