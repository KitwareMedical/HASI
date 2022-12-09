import { LitElement, html } from 'lit';
import { customElement } from 'lit/decorators.js';
import { literal } from 'lit/static-html.js';

import './biomarker-charts.js';
import './scan-table.js';
import './scan-views.js';

const SECTIONS = [
  { title: 'Scan Table', tag: literal`scan-table` },
  { title: 'Biomarker Charts', tag: literal`biomarker-charts` },
  { title: 'Features', tag: literal`scan-views` },
];

@customElement('population-root')
export class PopulationRoot extends LitElement {
  render() {
    return html` <accordion-layout .sections=${SECTIONS}></accordion-layout> `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'population-root': PopulationRoot;
  }
}
