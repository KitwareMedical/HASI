import { LitElement, css, html } from 'lit';
import { customElement, property } from 'lit/decorators.js';
import { Feature } from './scan.types';
import { ScanSelection } from './state/scan-selections';

@customElement('scan-view')
export class ScanView extends LitElement {
  @property() scan!: ScanSelection;
  @property() feature!: Feature;

  render() {
    return html`<div style="background-color: ${this.scan.color}">
      scanId: ${this.scan.id}, feature: ${this.feature}
    </div>`;
  }

  static styles = css`
    :host {
      text-align: center;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'scan-view': ScanView;
  }
}
