using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace CasiumGUI;

internal sealed class MainForm : Form
{
    // ── Controls ──────────────────────────────────────────────────────────
    private readonly TextBox            _txtCsvPath;
    private readonly Button             _btnBrowse;
    private readonly DataGridView       _grid;
    private readonly ComboBox           _cmbQuell;
    private readonly ComboBox           _cmbZiel;
    private readonly TextBox            _txtAlias;
    private readonly RadioButton        _rbCase, _rbDecode, _rbValues, _rbJson;
    private readonly TextBox            _txtElse;
    private readonly Button             _btnGenerate;
    private readonly RichTextBox        _rtbOutput;
    private readonly Button             _btnClipboard;
    private readonly Button             _btnSave;
    private readonly ToolStripStatusLabel _lblStatus;

    private CsvParser? _parser;

    // ── Constructor ───────────────────────────────────────────────────────
    public MainForm()
    {
        Text            = "Casium  —  SQL CASE Generator  v1.3.0";
        Size            = new Size(1080, 800);
        MinimumSize     = new Size(840, 640);
        StartPosition   = FormStartPosition.CenterScreen;
        AllowDrop       = true;
        Font            = new Font("Segoe UI", 9f);

        // ── Status strip ─────────────────────────────────────────────────
        var statusStrip = new StatusStrip { Dock = DockStyle.Bottom };
        _lblStatus = new ToolStripStatusLabel
        {
            Text      = "Bereit — CSV-Datei auswählen oder per Drag & Drop ins Fenster ziehen.",
            Spring    = true,
            TextAlign = ContentAlignment.MiddleLeft
        };
        statusStrip.Items.Add(_lblStatus);

        // ── Top panel: Dateiauswahl ───────────────────────────────────────
        var pnlTop = new Panel { Dock = DockStyle.Top, Height = 44, Padding = new Padding(8, 8, 8, 4) };

        var tblFile = new TableLayoutPanel
        {
            Dock        = DockStyle.Fill,
            ColumnCount = 3,
            RowCount    = 1
        };
        tblFile.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 78));
        tblFile.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        tblFile.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 130));
        tblFile.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        var lblFile = new Label { Text = "CSV-Datei:", Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft };
        _txtCsvPath = new TextBox { Dock = DockStyle.Fill, ReadOnly = true, BackColor = Color.White };
        _btnBrowse  = new Button  { Text = "Durchsuchen ...", Dock = DockStyle.Fill };
        _btnBrowse.Click += OnBrowse;

        tblFile.Controls.Add(lblFile,     0, 0);
        tblFile.Controls.Add(_txtCsvPath, 1, 0);
        tblFile.Controls.Add(_btnBrowse,  2, 0);
        pnlTop.Controls.Add(tblFile);

        // ── Bottom panel: SQL-Ausgabe ─────────────────────────────────────
        var tblBottom = new TableLayoutPanel
        {
            Dock      = DockStyle.Bottom,
            Height    = 255,
            RowCount  = 3,
            ColumnCount = 1,
            Padding   = new Padding(8, 0, 8, 4)
        };
        tblBottom.RowStyles.Add(new RowStyle(SizeType.Absolute, 22));   // Label
        tblBottom.RowStyles.Add(new RowStyle(SizeType.Percent,  100));  // RichTextBox
        tblBottom.RowStyles.Add(new RowStyle(SizeType.Absolute, 36));   // Buttons

        var lblOut = new Label
        {
            Text      = "SQL-Ausgabe:",
            Dock      = DockStyle.Fill,
            Font      = new Font("Segoe UI", 9f, FontStyle.Bold),
            TextAlign = ContentAlignment.BottomLeft
        };

        _rtbOutput = new RichTextBox
        {
            Dock        = DockStyle.Fill,
            Font        = new Font("Consolas", 9.5f),
            ReadOnly    = true,
            BackColor   = Color.FromArgb(30,  30,  30),
            ForeColor   = Color.FromArgb(220, 220, 220),
            ScrollBars  = RichTextBoxScrollBars.Both,
            WordWrap    = false
        };

        var pnlBtns = new FlowLayoutPanel
        {
            Dock          = DockStyle.Fill,
            FlowDirection = FlowDirection.LeftToRight,
            Padding       = new Padding(0, 4, 0, 0)
        };
        _btnClipboard = new Button { Text = "📋  In Zwischenablage kopieren", Width = 230, Height = 26, Enabled = false };
        _btnSave      = new Button { Text = "💾  Speichern als ...",          Width = 160, Height = 26, Enabled = false };
        _btnClipboard.Click += OnClipboard;
        _btnSave.Click      += OnSave;
        pnlBtns.Controls.AddRange(new Control[] { _btnClipboard, _btnSave });

        tblBottom.Controls.Add(lblOut,     0, 0);
        tblBottom.Controls.Add(_rtbOutput, 0, 1);
        tblBottom.Controls.Add(pnlBtns,   0, 2);

        // ── Middle: SplitContainer ────────────────────────────────────────
        var split = new SplitContainer { Dock = DockStyle.Fill };
        Load += (_, _) =>
        {
            split.Panel1MinSize = 280;
            split.Panel2MinSize = 270;
            try { split.SplitterDistance = split.Width / 2; } catch { }
        };

        // Linke Seite: CSV-Vorschau
        var lblGrid = new Label
        {
            Text      = "CSV-Vorschau  (max. 500 Zeilen):",
            Dock      = DockStyle.Top,
            Height    = 22,
            Font      = new Font("Segoe UI", 9f, FontStyle.Bold),
            TextAlign = ContentAlignment.BottomLeft,
            Padding   = new Padding(2, 0, 0, 0)
        };
        _grid = new DataGridView
        {
            Dock                              = DockStyle.Fill,
            ReadOnly                          = true,
            AllowUserToAddRows                = false,
            AllowUserToDeleteRows             = false,
            AllowUserToResizeRows             = false,
            RowHeadersVisible                 = false,
            AutoSizeColumnsMode               = DataGridViewAutoSizeColumnsMode.DisplayedCells,
            SelectionMode                     = DataGridViewSelectionMode.FullRowSelect,
            BackgroundColor                   = SystemColors.Window,
            BorderStyle                       = BorderStyle.None,
            ColumnHeadersHeightSizeMode       = DataGridViewColumnHeadersHeightSizeMode.AutoSize,
            ClipboardCopyMode                 = DataGridViewClipboardCopyMode.EnableWithoutHeaderText
        };
        split.Panel1.Padding = new Padding(8, 4, 4, 4);
        split.Panel1.Controls.Add(_grid);
        split.Panel1.Controls.Add(lblGrid);

        // Rechte Seite: Einstellungen
        var lblSettings = new Label
        {
            Text      = "Einstellungen:",
            Dock      = DockStyle.Top,
            Height    = 22,
            Font      = new Font("Segoe UI", 9f, FontStyle.Bold),
            TextAlign = ContentAlignment.BottomLeft,
            Padding   = new Padding(2, 0, 0, 0)
        };

        var tblSettings = new TableLayoutPanel
        {
            Dock        = DockStyle.Fill,
            ColumnCount = 2,
            RowCount    = 8,
            Padding     = new Padding(4, 4, 8, 4)
        };
        tblSettings.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 92));
        tblSettings.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        tblSettings.RowStyles.Add(new RowStyle(SizeType.Absolute, 34));   // Quellspalte
        tblSettings.RowStyles.Add(new RowStyle(SizeType.Absolute, 34));   // Zielspalte
        tblSettings.RowStyles.Add(new RowStyle(SizeType.Absolute, 34));   // SQL-Alias
        tblSettings.RowStyles.Add(new RowStyle(SizeType.Absolute, 108));  // Format
        tblSettings.RowStyles.Add(new RowStyle(SizeType.Absolute, 34));   // ELSE-Wert
        tblSettings.RowStyles.Add(new RowStyle(SizeType.Absolute, 18));   // Hint
        tblSettings.RowStyles.Add(new RowStyle(SizeType.Percent, 100));   // Spacer
        tblSettings.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));   // Generieren

        void AddRow(int row, string labelText, Control ctrl)
        {
            tblSettings.Controls.Add(new Label
            {
                Text      = labelText,
                Dock      = DockStyle.Fill,
                TextAlign = ContentAlignment.MiddleLeft
            }, 0, row);
            tblSettings.Controls.Add(ctrl, 1, row);
        }

        _cmbQuell = new ComboBox { Dock = DockStyle.Fill, DropDownStyle = ComboBoxStyle.DropDownList, Margin = new Padding(0, 5, 0, 0) };
        _cmbZiel  = new ComboBox { Dock = DockStyle.Fill, DropDownStyle = ComboBoxStyle.DropDownList, Margin = new Padding(0, 5, 0, 0) };
        _txtAlias = new TextBox  { Dock = DockStyle.Fill, Margin = new Padding(0, 5, 0, 0) };
        _txtElse  = new TextBox  { Dock = DockStyle.Fill, Margin = new Padding(0, 5, 0, 0), Text = "Unbekannt" };

        AddRow(0, "Quellspalte:", _cmbQuell);
        AddRow(1, "Zielspalte:",  _cmbZiel);
        AddRow(2, "SQL-Alias:",   _txtAlias);

        // Format-Auswahl
        var pnlFormats = new FlowLayoutPanel
        {
            Dock          = DockStyle.Fill,
            FlowDirection = FlowDirection.TopDown,
            WrapContents  = false,
            Margin        = new Padding(0, 2, 0, 0)
        };
        _rbCase   = new RadioButton { Text = "SQL CASE",        Checked = true, AutoSize = true };
        _rbDecode = new RadioButton { Text = "DECODE (Oracle)", AutoSize = true };
        _rbValues = new RadioButton { Text = "VALUES-Tabelle",  AutoSize = true };
        _rbJson   = new RadioButton { Text = "JSON",            AutoSize = true };
        foreach (var rb in new[] { _rbCase, _rbDecode, _rbValues, _rbJson })
            rb.CheckedChanged += OnFormatChanged;
        pnlFormats.Controls.AddRange(new Control[] { _rbCase, _rbDecode, _rbValues, _rbJson });
        AddRow(3, "Format:", pnlFormats);

        AddRow(4, "ELSE-Wert:", _txtElse);

        // Hint unter ELSE
        tblSettings.Controls.Add(new Label(), 0, 5);
        tblSettings.Controls.Add(new Label
        {
            Text      = "leer lassen für NULL",
            ForeColor = Color.Gray,
            Font      = new Font("Segoe UI", 7.5f),
            Dock      = DockStyle.Fill
        }, 1, 5);

        // Spacer (row 6 — leer)

        // Generieren-Button
        _btnGenerate = new Button
        {
            Text       = "▶   Generieren",
            Dock       = DockStyle.Fill,
            Enabled    = false,
            Margin     = new Padding(0, 6, 0, 6),
            BackColor  = Color.FromArgb(0, 122, 204),
            ForeColor  = Color.White,
            FlatStyle  = FlatStyle.Flat,
            Font       = new Font("Segoe UI", 10f, FontStyle.Bold),
            Cursor     = Cursors.Hand
        };
        _btnGenerate.FlatAppearance.BorderSize = 0;
        _btnGenerate.Click += OnGenerate;
        tblSettings.SetColumnSpan(_btnGenerate, 2);
        tblSettings.Controls.Add(_btnGenerate, 0, 7);

        split.Panel2.Padding = new Padding(4, 4, 4, 4);
        split.Panel2.Controls.Add(tblSettings);
        split.Panel2.Controls.Add(lblSettings);

        // ── Drag & Drop ───────────────────────────────────────────────────
        DragEnter += (_, e) =>
        {
            if (e.Data?.GetDataPresent(DataFormats.FileDrop) == true)
                e.Effect = DragDropEffects.Copy;
        };
        DragDrop += (_, e) =>
        {
            if (e.Data?.GetData(DataFormats.FileDrop) is string[] files && files.Length > 0)
                LoadCsv(files[0]);
        };

        // ── Zusammenbauen ─────────────────────────────────────────────────
        // Reihenfolge wichtig: Fill-Control zuletzt, damit es den Rest ausfüllt
        Controls.Add(split);
        Controls.Add(tblBottom);
        Controls.Add(pnlTop);
        Controls.Add(statusStrip);
    }

    // ── Event handlers ────────────────────────────────────────────────────

    private void OnFormatChanged(object? sender, EventArgs e)
    {
        _txtElse.Enabled = _rbCase.Checked || _rbDecode.Checked;
    }

    private void OnBrowse(object? sender, EventArgs e)
    {
        using var dlg = new OpenFileDialog
        {
            Title  = "CSV-Datei auswählen",
            Filter = "CSV-Dateien (*.csv)|*.csv|Alle Dateien (*.*)|*.*"
        };
        if (dlg.ShowDialog() == DialogResult.OK) LoadCsv(dlg.FileName);
    }

    private void LoadCsv(string path)
    {
        try
        {
            var parser = new CsvParser();
            parser.Parse(path);
            _parser = parser;
            _txtCsvPath.Text = path;

            // Grid befüllen (max. 500 Zeilen als Vorschau)
            _grid.Columns.Clear();
            foreach (var h in _parser.Headers)
                _grid.Columns.Add(new DataGridViewTextBoxColumn
                {
                    HeaderText = h,
                    SortMode   = DataGridViewColumnSortMode.NotSortable
                });

            _grid.Rows.Clear();
            foreach (var rec in _parser.Records.Take(500))
            {
                var cells = new object[_parser.Headers.Length];
                for (int i = 0; i < _parser.Headers.Length; i++)
                    cells[i] = i < rec.Fields.Length ? rec.Fields[i] : "";
                _grid.Rows.Add(cells);
            }

            // ComboBoxen befüllen
            _cmbQuell.Items.Clear();
            _cmbZiel.Items.Clear();
            for (int i = 0; i < _parser.Headers.Length; i++)
            {
                string entry = $"[{i}]  {_parser.Headers[i]}";
                _cmbQuell.Items.Add(entry);
                _cmbZiel.Items.Add(entry);
            }
            _cmbQuell.SelectedIndex = 0;
            _cmbZiel.SelectedIndex  = Math.Min(1, _parser.Headers.Length - 1);

            _btnGenerate.Enabled = true;
            _lblStatus.Text = $"Geladen: {_parser.Records.Count} Datenzeilen  |  " +
                              $"Trennzeichen: '{_parser.Separator}'  |  " +
                              $"{_parser.Headers.Length} Spalten";
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Fehler beim Laden:\n{ex.Message}", "Fehler",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
            _lblStatus.Text = "Fehler beim Laden der Datei.";
        }
    }

    private void OnGenerate(object? sender, EventArgs e)
    {
        if (_parser is null) return;

        if (string.IsNullOrWhiteSpace(_txtAlias.Text))
        {
            MessageBox.Show("Bitte einen SQL-Alias eingeben.", "Hinweis",
                MessageBoxButtons.OK, MessageBoxIcon.Information);
            _txtAlias.Focus();
            return;
        }

        int idxQ = _cmbQuell.SelectedIndex;
        int idxZ = _cmbZiel.SelectedIndex;
        if (idxQ < 0 || idxZ < 0) return;

        var format = _rbCase.Checked   ? OutputFormat.Case
                   : _rbDecode.Checked ? OutputFormat.Decode
                   : _rbValues.Checked ? OutputFormat.Values
                                       : OutputFormat.Json;

        string elseVal = (_rbCase.Checked || _rbDecode.Checked) ? _txtElse.Text.Trim() : "";
        string srcCol  = _parser.Headers[idxQ];
        string alias   = _txtAlias.Text.Trim();

        // Einträge sammeln: leere überspringen, Duplikate deduplizieren
        var seen    = new HashSet<string>(StringComparer.Ordinal);
        var entries = new List<MappingEntry>();
        int skippedEmpty = 0, skippedDup = 0;

        foreach (var rec in _parser.Records)
        {
            if (rec.Fields.Length <= idxQ || rec.Fields.Length <= idxZ) continue;
            string q = rec.Fields[idxQ], z = rec.Fields[idxZ];
            if (string.IsNullOrEmpty(q) || string.IsNullOrEmpty(z)) { skippedEmpty++; continue; }
            if (!seen.Add(q)) { skippedDup++; continue; }
            entries.Add(new MappingEntry(q, z));
        }

        _rtbOutput.Text       = SqlGenerator.Generate(entries, srcCol, alias, elseVal, format);
        _btnClipboard.Enabled = true;
        _btnSave.Enabled      = true;

        _lblStatus.Text = $"{entries.Count} Einträge generiert  |  " +
                          $"{skippedEmpty} leer übersprungen  |  " +
                          $"{skippedDup} Duplikate übersprungen";
    }

    private void OnClipboard(object? sender, EventArgs e)
    {
        if (string.IsNullOrEmpty(_rtbOutput.Text)) return;
        Clipboard.SetText(_rtbOutput.Text);
        _lblStatus.Text = "SQL-Ausdruck in die Zwischenablage kopiert.";
    }

    private void OnSave(object? sender, EventArgs e)
    {
        using var dlg = new SaveFileDialog
        {
            Title    = "SQL-Ausgabe speichern",
            FileName = "FertigerCase.txt",
            Filter   = "Textdateien (*.txt)|*.txt|SQL-Dateien (*.sql)|*.sql|Alle Dateien (*.*)|*.*"
        };
        if (dlg.ShowDialog() != DialogResult.OK) return;
        File.WriteAllText(dlg.FileName, _rtbOutput.Text, Encoding.UTF8);
        _lblStatus.Text = $"Gespeichert: {dlg.FileName}";
    }
}
