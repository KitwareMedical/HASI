import { LitElement, css, html, unsafeCSS } from 'lit';
import { customElement } from 'lit/decorators.js';
import {
  BasicKeyHandler,
  BasicMouseHandler,
  BasicSelectionModel,
  CellGroup,
  CellRenderer,
  DataGrid,
  DataModel,
  GraphicsContext,
  ResizeHandle,
  SelectionModel,
  TextRenderer,
} from '@lumino/datagrid';
import { StackedPanel, Widget } from '@lumino/widgets';
import { Platform } from '@lumino/domutils';

import luminoStyles from '@lumino/default-theme/style/index.css?inline';

import { ContextConsumer } from '@lit-labs/context';
import { hasiContext, HasiContext, ScanClicked } from './state/hasi.machine.js';

import { fields, ScanId } from './scan.types.js';
import { Drag } from '@lumino/dragdrop';
import { EventObject } from 'xstate';

interface RowValue {
  id: ScanId;
  selected: boolean;
}

class LargeDataModel extends DataModel {
  selectedScans: Set<ScanId>;

  constructor(selectedScans: Set<ScanId>) {
    super();
    this.selectedScans = selectedScans;
  }

  rowCount(region: DataModel.RowRegion): number {
    return region === 'body' ? 1000000000000 : 1;
  }

  columnCount(region: DataModel.ColumnRegion): number {
    return region === 'body' ? fields.length : 1;
  }

  data(
    region: DataModel.CellRegion,
    row: number,
    column: number
  ): RowValue | string {
    if (region === 'row-header') {
      const id = `${row}`;
      return { id, selected: this.selectedScans.has(id) };
    }
    if (region === 'column-header') {
      return `${fields[column]}`;
    }
    if (region === 'corner-header') {
      return ``;
    }
    return `(${row}, ${column})`;
  }

  scanUpdated(id: ScanId) {
    const row = Number(id);
    this.emitChanged({
      type: 'cells-changed',
      region: 'row-header',
      row,
      rowSpan: 1,
      column: 0,
      columnSpan: 1,
    });
  }
}

class CheckboxRenderer extends TextRenderer {
  readonly checked: CellRenderer.ConfigOption<boolean>;
  readonly id: CellRenderer.ConfigOption<ScanId>;

  constructor(options: CheckboxRenderer.IOptions) {
    super(options);
    this.checked = options.checked;
    this.id = options.id;
  }

  drawBackground(gc: GraphicsContext, config: CellRenderer.CellConfig): void {
    const checked = CellRenderer.resolveOption(this.checked, config);
    if (checked) {
      gc.fillStyle = '#00FF0040';
      gc.fillRect(config.x, config.y, config.width, config.height - 1);
    }
  }

  format = ({ value }: { value: RowValue }) => {
    return value.selected ? '\u2713' : '\u2610';
  };
}

namespace CheckboxRenderer {
  export interface IOptions extends TextRenderer.IOptions {
    checked: CellRenderer.ConfigOption<boolean>;
    id: CellRenderer.ConfigOption<ScanId>;
  }
}

class CheckboxMouseHandler extends BasicMouseHandler {
  private stateService: ContextConsumer<
    {
      __context__: HasiContext;
    },
    any
  >;

  constructor(
    stateService: ContextConsumer<
      {
        __context__: HasiContext;
      },
      any
    >
  ) {
    super();
    this.stateService = stateService;
  }

  // helper for onDown
  resizeHandleForHitTest(hit: DataGrid.HitTestResult): ResizeHandle {
    // Fetch the row and column.
    let r = hit.row;
    let c = hit.column;

    // Fetch the leading and trailing sizes.
    let lw = hit.x;
    let lh = hit.y;
    let tw = hit.width - hit.x;
    let th = hit.height - hit.y;

    // Set up the result variable.
    let result: ResizeHandle;

    // Dispatch based on hit test region.
    switch (hit.region) {
      case 'corner-header':
        if (c > 0 && lw <= 5) {
          result = 'left';
        } else if (tw <= 6) {
          result = 'right';
        } else if (r > 0 && lh <= 5) {
          result = 'top';
        } else if (th <= 6) {
          result = 'bottom';
        } else {
          result = 'none';
        }
        break;
      case 'column-header':
        if (c > 0 && lw <= 5) {
          result = 'left';
        } else if (tw <= 6) {
          result = 'right';
        } else if (r > 0 && lh <= 5) {
          result = 'top';
        } else if (th <= 6) {
          result = 'bottom';
        } else {
          result = 'none';
        }
        break;
      case 'row-header':
        if (c > 0 && lw <= 5) {
          result = 'left';
        } else if (tw <= 6) {
          result = 'right';
        } else if (r > 0 && lh <= 5) {
          result = 'top';
        } else if (th <= 6) {
          result = 'bottom';
        } else {
          result = 'none';
        }
        break;
      case 'body':
        result = 'none';
        break;
      case 'void':
        result = 'none';
        break;
      default:
        throw 'unreachable';
    }

    // Return the result.
    return result;
  }

  createCellConfigObject(
    grid: DataGrid,
    hit: DataGrid.HitTestResult
  ): CellRenderer.CellConfig | undefined {
    const { region, row, column } = hit;

    // Terminate call if region is void.
    if (region === 'void') {
      return undefined;
    }

    // Augment hit region params with value and metadata.
    const value = grid.dataModel!.data(region, row, column);
    const metadata = grid.dataModel!.metadata(region, row, column);

    // Create cell config object to retrieve cell renderer.
    const config = {
      ...hit,
      value: value,
      metadata: metadata,
    } as CellRenderer.CellConfig;

    return config;
  }

  onMouseDown(grid: DataGrid, event: MouseEvent): void {
    // Unpack the event.
    let { clientX, clientY } = event;

    // Hit test the grid.
    let hit = grid.hitTest(clientX, clientY);

    // Unpack the hit test.
    const { region, row, column } = hit;

    // Bail if the hit test is on an uninteresting region.
    if (region === 'void') {
      return;
    }

    // Fetch the modifier flags.
    let shift = event.shiftKey;
    let accel = Platform.accelKey(event);

    // If the hit test is the body region, the only option is select.
    if (region === 'body') {
      // Fetch the selection model.
      let model = grid.selectionModel;

      // Bail early if there is no selection model.
      if (!model) {
        return;
      }

      // Override the document cursor.
      let override = Drag.overrideCursor('default');

      // Set up the press data.
      this._pressData = {
        type: 'select',
        region,
        row,
        column,
        override,
        localX: -1,
        localY: -1,
        timeout: -1,
      };

      // Set up the selection variables.
      let r1: number;
      let c1: number;
      let r2: number;
      let c2: number;
      let cursorRow: number;
      let cursorColumn: number;
      let clear: SelectionModel.ClearMode;

      // Accel == new selection, keep old selections.
      if (accel) {
        r1 = row;
        r2 = row;
        c1 = column;
        c2 = column;
        cursorRow = row;
        cursorColumn = column;
        clear = 'none';
      } else if (shift) {
        r1 = model.cursorRow;
        r2 = row;
        c1 = model.cursorColumn;
        c2 = column;
        cursorRow = model.cursorRow;
        cursorColumn = model.cursorColumn;
        clear = 'current';
      } else {
        r1 = row;
        r2 = row;
        c1 = column;
        c2 = column;
        cursorRow = row;
        cursorColumn = column;
        clear = 'all';
      }

      // Make the selection.
      model.select({ r1, c1, r2, c2, cursorRow, cursorColumn, clear });

      // Done.
      return;
    }

    // Otherwise, the hit test is on a header region.

    // Convert the hit test into a part.
    let handle = this.resizeHandleForHitTest(hit);

    // Fetch the cursor for the handle.
    let cursor = this.cursorForHandle(handle);

    // Handle horizontal resize.
    if (handle === 'left' || handle === 'right') {
      // Set up the resize data type.
      const type = 'column-resize';

      // Determine the column region.
      let rgn: DataModel.ColumnRegion =
        region === 'column-header' ? 'body' : 'row-header';

      // Determine the section index.
      let index = handle === 'left' ? column - 1 : column;

      // Fetch the section size.
      let size = grid.columnSize(rgn, index);

      // Override the document cursor.
      let override = Drag.overrideCursor(cursor);

      // Create the temporary press data.
      this._pressData = { type, region: rgn, index, size, clientX, override };

      // Done.
      return;
    }

    // Handle vertical resize
    if (handle === 'top' || handle === 'bottom') {
      // Set up the resize data type.
      const type = 'row-resize';

      // Determine the row region.
      let rgn: DataModel.RowRegion =
        region === 'row-header' ? 'body' : 'column-header';

      // Determine the section index.
      let index = handle === 'top' ? row - 1 : row;

      // Fetch the section size.
      let size = grid.rowSize(rgn, index);

      // Override the document cursor.
      let override = Drag.overrideCursor(cursor);

      // Create the temporary press data.
      this._pressData = { type, region: rgn, index, size, clientY, override };

      // Done.
      return;
    }

    // Check for clicking row header
    if (grid) {
      // Create cell config object.
      const config = this.createCellConfigObject(grid, hit);

      // Retrieve cell renderer.
      let renderer = grid.cellRenderers.get(config!);

      if (renderer instanceof CheckboxRenderer) {
        const id = CellRenderer.resolveOption(renderer.id, config!);
        if (id && this.stateService.value) {
          this.stateService.value?.service.send({ type: 'SCAN_CLICKED', id });
          return;
        } else {
          throw new Error('Did not find ID or stateService not defined');
        }
      }
    }

    // Otherwise, the only option is select.

    // Fetch the selection model.
    let model = grid.selectionModel;

    // Bail if there is no selection model.
    if (!model) {
      return;
    }

    // Override the document cursor.
    let override = Drag.overrideCursor('default');

    // Set up the press data.
    this._pressData = {
      type: 'select',
      region,
      row,
      column,
      override,
      localX: -1,
      localY: -1,
      timeout: -1,
    };

    // Set up the selection variables.
    let r1: number;
    let c1: number;
    let r2: number;
    let c2: number;
    let cursorRow: number;
    let cursorColumn: number;
    let clear: SelectionModel.ClearMode;

    // Compute the selection based on the pressed region.
    if (region === 'corner-header') {
      r1 = 0;
      r2 = Infinity;
      c1 = 0;
      c2 = Infinity;
      cursorRow = accel ? 0 : shift ? model.cursorRow : 0;
      cursorColumn = accel ? 0 : shift ? model.cursorColumn : 0;
      clear = accel ? 'none' : shift ? 'current' : 'all';
    } else if (region === 'row-header') {
      r1 = accel ? row : shift ? model.cursorRow : row;
      r2 = row;

      const selectionGroup: CellGroup = { r1: r1, c1: 0, r2: r2, c2: 0 };
      const joinedGroup = CellGroup.joinCellGroupsIntersectingAtAxis(
        grid.dataModel!,
        ['row-header', 'body'],
        'row',
        selectionGroup
      );
      // Check if there are any merges
      if (joinedGroup.r1 != Number.MAX_VALUE) {
        r1 = joinedGroup.r1;
        r2 = joinedGroup.r2;
      }

      c1 = 0;
      c2 = Infinity;
      cursorRow = accel ? row : shift ? model.cursorRow : row;
      cursorColumn = accel ? 0 : shift ? model.cursorColumn : 0;
      clear = accel ? 'none' : shift ? 'current' : 'all';
    } else if (region === 'column-header') {
      r1 = 0;
      r2 = Infinity;
      c1 = accel ? column : shift ? model.cursorColumn : column;
      c2 = column;

      const selectionGroup: CellGroup = { r1: 0, c1: c1, r2: 0, c2: c2 };
      const joinedGroup = CellGroup.joinCellGroupsIntersectingAtAxis(
        grid.dataModel!,
        ['column-header', 'body'],
        'column',
        selectionGroup
      );
      // Check if there are any merges
      if (joinedGroup.c1 != Number.MAX_VALUE) {
        c1 = joinedGroup.c1;
        c2 = joinedGroup.c2;
      }

      cursorRow = accel ? 0 : shift ? model.cursorRow : 0;
      cursorColumn = accel ? column : shift ? model.cursorColumn : column;
      clear = accel ? 'none' : shift ? 'current' : 'all';
    } else {
      r1 = accel ? row : shift ? model.cursorRow : row;
      r2 = row;
      c1 = accel ? column : shift ? model.cursorColumn : column;
      c2 = column;
      cursorRow = accel ? row : shift ? model.cursorRow : row;
      cursorColumn = accel ? column : shift ? model.cursorColumn : column;
      clear = accel ? 'none' : shift ? 'current' : 'all';
    }

    // Make the selection.
    model.select({ r1, c1, r2, c2, cursorRow, cursorColumn, clear });
  }
}

const ro = new ResizeObserver((entries) => {
  entries.forEach((entry) => (entry.target as ScanTable).resizeHandler());
});

@customElement('scan-table')
export class ScanTable extends LitElement {
  private dataModel!: LargeDataModel;
  private _grid!: DataGrid;
  private _wrapper!: StackedPanel;

  public stateService = new ContextConsumer(this, hasiContext, undefined, true);

  scanClickedHandler = (e: EventObject) => {
    if (e.type === 'SCAN_CLICKED')
      this.dataModel.scanUpdated((e as ScanClicked).id);
  };

  resizeHandler = () => {
    this._wrapper.update();
  };

  connectedCallback() {
    super.connectedCallback();
    const blueStripeStyle: DataGrid.Style = {
      ...DataGrid.defaultStyle,
      rowBackgroundColor: (i) =>
        i % 2 === 0 ? 'rgba(138, 172, 200, 0.3)' : '',
      columnBackgroundColor: (i) =>
        i % 2 === 0 ? 'rgba(100, 100, 100, 0.1)' : '',
    };

    this._grid = new DataGrid({
      style: blueStripeStyle,
      defaultSizes: {
        rowHeight: 32,
        columnWidth: 90,
        rowHeaderWidth: 30,
        columnHeaderHeight: 32,
      },
    });
    const dataModel = new LargeDataModel(
      this.stateService.value!.service.machine.context.selectedScans
    );
    this.dataModel = dataModel;

    this._grid.dataModel = dataModel;
    this.stateService.value!.service.onEvent(this.scanClickedHandler);
    this._grid.keyHandler = new BasicKeyHandler();
    this._grid.mouseHandler = new CheckboxMouseHandler(this.stateService);
    this._grid.selectionModel = new BasicSelectionModel({
      dataModel,
    });
    const checkboxRenderer = new CheckboxRenderer({
      id: (config: CellRenderer.CellConfig) => config.value.id,
      checked: (config: CellRenderer.CellConfig) => config.value.selected,
    });
    this._grid.cellRenderers.update({
      'row-header': () => checkboxRenderer,
    });

    this._wrapper = new StackedPanel();
    this._wrapper.addWidget(this._grid);
    Widget.attach(this._wrapper, this.renderRoot as HTMLElement);

    ro.observe(this);
  }

  disconnectedCallback() {
    this.stateService.value!.service.off(this.scanClickedHandler);
    ro.unobserve(this);
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
    'scan-table': ScanTable;
  }
}
