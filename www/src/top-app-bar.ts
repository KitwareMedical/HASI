import { LitElement, css, html } from 'lit';
import { customElement, property } from 'lit/decorators.js';
import '@material/web/button/outlined-button';
import '@material/web/iconbutton/standard-icon-button-toggle';

@customElement('top-app-bar')
export class TopAppBar extends LitElement {
  @property() title: string = 'title';
  @property() isMenuOpen: boolean = true;
  render() {
    return html`
      <md-standard-icon-button-toggle
        .isOn=${this.isMenuOpen}
        @click="${this._clickHandler}"
        onIcon="close"
        offIcon="menu"
      >
      </md-standard-icon-button-toggle>
      <h1>${this.title}</h1>
    `;
  }
  static styles = css`
    :host {
      display: flex;
    }
  `;

  private _clickHandler() {
    this.dispatchEvent(
      new CustomEvent('toggleMenu', {
        bubbles: true,
        composed: true,
      })
    );
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'top-app-bar': TopAppBar;
  }
}