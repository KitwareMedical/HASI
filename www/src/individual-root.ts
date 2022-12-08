import { LitElement, html } from 'lit';
import { customElement } from 'lit/decorators.js';
import { literal } from 'lit/static-html.js';

import './accordion-layout.js';
import './scan-table.js';
import './scan-views.js';

const SECTIONS = [
  { title: 'Scan Table', tag: literal`scan-table` },
  { title: 'Features', tag: literal`scan-views` },
];

@customElement('individual-root')
export class IndividualRoot extends LitElement {
  render() {
    return html` <accordion-layout .sections=${SECTIONS}></accordion-layout> `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'individual-root': IndividualRoot;
  }
}
