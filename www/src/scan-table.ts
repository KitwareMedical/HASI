import { LitElement, css, html, unsafeCSS } from "lit";
import { customElement } from "lit/decorators.js";
import {
  BasicKeyHandler,
  BasicMouseHandler,
  BasicSelectionModel,
  DataGrid,
  DataModel,
} from "@lumino/datagrid";

import { StackedPanel, Widget } from "@lumino/widgets";

import luminoStyles from "@lumino/default-theme/style/index.css?inline";

class LargeDataModel extends DataModel {
  rowCount(region: DataModel.RowRegion): number {
    return region === "body" ? 1000000000000 : 2;
  }

  columnCount(region: DataModel.ColumnRegion): number {
    return region === "body" ? 1000000000000 : 3;
  }

  data(region: DataModel.CellRegion, row: number, column: number): any {
    if (region === "row-header") {
      return `R: ${row}, ${column}`;
    }
    if (region === "column-header") {
      return `C: ${row}, ${column}`;
    }
    if (region === "corner-header") {
      return `N: ${row}, ${column}`;
    }
    return `(${row}, ${column})`;
  }
}

@customElement("scan-table")
export class ScanTable extends LitElement {
  private _grid: DataGrid;
  private _wrapper: StackedPanel;

  constructor() {
    super();
    const blueStripeStyle: DataGrid.Style = {
      ...DataGrid.defaultStyle,
      rowBackgroundColor: (i) =>
        i % 2 === 0 ? "rgba(138, 172, 200, 0.3)" : "",
      columnBackgroundColor: (i) =>
        i % 2 === 0 ? "rgba(100, 100, 100, 0.1)" : "",
    };

    this._grid = new DataGrid({ style: blueStripeStyle });
    this._grid.dataModel = new LargeDataModel();
    this._grid.keyHandler = new BasicKeyHandler();
    this._grid.mouseHandler = new BasicMouseHandler();
    this._grid.selectionModel = new BasicSelectionModel({
      dataModel: this._grid.dataModel,
    });

    this._wrapper = new StackedPanel();
    this._wrapper.addWidget(this._grid);
  }

  _resizeHandler = () => {
    this._wrapper.update();
  };

  connectedCallback() {
    super.connectedCallback();
    Widget.attach(this._wrapper, this.renderRoot as HTMLElement);
    window.addEventListener("resize", this._resizeHandler);
  }

  disconnectedCallback() {
    window.removeEventListener("resize", this._resizeHandler);
    super.disconnectedCallback();
  }

  render() {
    return html``;
  }

  static styles = [
    css`
      :host {
        margin-bottom: 1rem;
      }

      :host > * {
        height: 100%;
      }
    `,
    unsafeCSS(luminoStyles),
  ];
}

declare global {
  interface HTMLElementTagNameMap {
    "scan-table": ScanTable;
  }
}
