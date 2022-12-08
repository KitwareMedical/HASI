import { LitElement, html, css } from 'lit';
import { customElement, property } from 'lit/decorators.js';
import '@material/web/navigationdrawer/navigation-drawer.js';
import '@material/web/iconbutton/filled-icon-button-toggle.js';

import { isOnPath, PAGES } from './pages';
import { Routes } from '@lit-labs/router';

type Pages = keyof typeof PAGES;
const pagesToIcons: Record<Pages, string> = {
  population: 'groups',
  individual: 'person',
  processing: 'cloud_upload',
};

/**
 * List of page links
 *
 */
@customElement('nav-menu')
export class NavMenu extends LitElement {
  @property() opened = true;
  @property() routes!: Routes;

  render() {
    return html`
      <md-navigation-drawer .opened=${this.opened}>
        <div class="nav-items">
          ${Object.entries(PAGES)
            .map(([page, values]) => ({
              icon: pagesToIcons[page as Pages],
              ...values,
            }))
            .map(({ icon, path, title }) => {
              console.log(isOnPath(path));
              return html`
                <a href=${this.routes.link(path)}>
                  <md-filled-icon-button-toggle
                    .isOn=${isOnPath(path)}
                    onIcon="${icon}"
                    offIcon="${icon}"
                  ></md-filled-icon-button-toggle>
                  <div>${title}</div>
                </a>
              `;
            })}
        </div>
      </md-navigation-drawer>
    `;
  }

  static styles = css`
    md-navigation-drawer {
      height: 100%;
      --md-navigation-drawer-container-width: 120px;
      --md-navigation-drawer-container-color: var(--text-primary);
      --md-navigation-drawer-container-shape-start-end: 0;
      --md-navigation-drawer-container-shape-end-end: 0;
      overflow: auto;
    }

    .nav-items {
      padding-top: 1rem;
      display: flex;
      flex-direction: column;
      align-items: center;
    }

    a {
      text-decoration: none;
      display: flex;
      flex-direction: column;
      align-items: center;
      margin-bottom: 2rem;
      color: var(--text-white);
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'nav-menu': NavMenu;
  }
}
