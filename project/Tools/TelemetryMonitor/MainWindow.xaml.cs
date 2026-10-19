using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Media;

using System.ComponentModel;
using System.Windows.Data;

namespace TelemetryMonitor
{
    public class MetricItem : System.ComponentModel.INotifyPropertyChanged
    {
        public event System.ComponentModel.PropertyChangedEventHandler? PropertyChanged;
        private string _key = string.Empty;
        public string Key 
        { 
            get => _key;
            set
            {
                _key = value;
                if (!string.IsNullOrEmpty(_key))
                {
                    int slashIndex = _key.IndexOf('/');
                    if (slashIndex >= 0)
                    {
                        Category = _key.Substring(0, slashIndex);
                        Name = _key.Substring(slashIndex + 1);
                    }
                    else
                    {
                        Category = "Uncategorized";
                        Name = _key;
                    }
                }
            }
        }
        
        public string Category { get; private set; } = "Uncategorized";
        public string Name { get; private set; } = string.Empty;
        
        public int DisplayOrder 
        { 
            get 
            {
                if (Category == "System") return 0;
                if (Category == "GPU_Particle") return 1;
                if (Category == "Voxel_Particle") return 2;
                return 3;
            }
        }
        
        private string _value = string.Empty;
        public string Value
        {
            get => _value;
            set
            {
                if (_value != value)
                {
                    _value = value;
                    PropertyChanged?.Invoke(this, new System.ComponentModel.PropertyChangedEventArgs(nameof(Value)));
                    UpdateStatusColor();
                }
            }
        }

        private Brush _statusColorBrush = new SolidColorBrush(Color.FromRgb(255, 255, 255));
        public Brush StatusColorBrush
        {
            get => _statusColorBrush;
            set
            {
                if (_statusColorBrush != value)
                {
                    _statusColorBrush = value;
                    PropertyChanged?.Invoke(this, new System.ComponentModel.PropertyChangedEventArgs(nameof(StatusColorBrush)));
                }
            }
        }

        private void UpdateStatusColor()
        {
            // ターゲットFPSをとりあえず60 (1フレームの限界 = 16.66ms) とする
            double targetFrameTime = 16.66;
            
            if (double.TryParse(Value, out double dVal))
            {
                if (Key.Contains("FPS", StringComparison.OrdinalIgnoreCase))
                {
                    // FPS: 高いほど良い
                    if (dVal >= 58.0) StatusColorBrush = new SolidColorBrush(Color.FromRgb(0, 255, 157)); // Green
                    else if (dVal >= 30.0) StatusColorBrush = new SolidColorBrush(Color.FromRgb(255, 204, 0)); // Yellow
                    else StatusColorBrush = new SolidColorBrush(Color.FromRgb(255, 51, 102)); // Red
                }
                else if (Key.Contains("FrameTime", StringComparison.OrdinalIgnoreCase))
                {
                    // FrameTimeは60FPS制限により16.66ms付近に張り付くため、若干のブレ(17.0ms)まで正常(Green)とする
                    if (dVal <= targetFrameTime + 0.5) StatusColorBrush = new SolidColorBrush(Color.FromRgb(0, 255, 157)); // Green
                    else StatusColorBrush = new SolidColorBrush(Color.FromRgb(255, 51, 102)); // Red
                }
                else if (Key.Equals("System/CPU_Time_ms", StringComparison.OrdinalIgnoreCase) || Key.Equals("System/GPU_Time_ms", StringComparison.OrdinalIgnoreCase))
                {
                    // 処理時間(ms): CPU/GPUの純粋な負荷。低いほど良い。16.66msが限界。
                    if (dVal < targetFrameTime * 0.6) StatusColorBrush = new SolidColorBrush(Color.FromRgb(0, 255, 157)); // 余裕 (Green)
                    else if (dVal <= targetFrameTime) StatusColorBrush = new SolidColorBrush(Color.FromRgb(255, 204, 0)); // 限界に近い (Yellow)
                    else StatusColorBrush = new SolidColorBrush(Color.FromRgb(255, 51, 102)); // 限界突破/処理落ち (Red)
                }
                else if (Key.Contains("ThreadPool", StringComparison.OrdinalIgnoreCase))
                {
                    StatusColorBrush = new SolidColorBrush(Color.FromRgb(0, 204, 255)); // Cyan
                }
                else if (Key.Contains("Particle", StringComparison.OrdinalIgnoreCase))
                {
                    StatusColorBrush = new SolidColorBrush(Color.FromRgb(204, 153, 255)); // Light Purple
                }
                else
                {
                    StatusColorBrush = new SolidColorBrush(Color.FromRgb(255, 255, 255)); // White
                }
            }
        }
    }

    public partial class MainWindow : Window
    {
        private ObservableCollection<MetricItem> _metrics = new ObservableCollection<MetricItem>();
        private UdpClient? _udpClient;

        // グラフ描画用履歴
        private const int MaxHistory = 300;
        private Queue<double> _fpsHistory = new Queue<double>();
        private Queue<double> _cpuHistory = new Queue<double>();
        private Queue<double> _gpuHistory = new Queue<double>();

        public MainWindow()
        {
            InitializeComponent();
            
            var view = CollectionViewSource.GetDefaultView(_metrics);
            view.GroupDescriptions.Add(new PropertyGroupDescription("Category"));
            view.SortDescriptions.Add(new SortDescription("DisplayOrder", ListSortDirection.Ascending));
            view.SortDescriptions.Add(new SortDescription("Name", ListSortDirection.Ascending));
            
            MetricsList.ItemsSource = view;
            StartListening();
            StartCommandClient();
        }

        private void StartListening()
        {
            try
            {
                _udpClient = new UdpClient(8888);
                Task.Run(ReceiveDataAsync);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to start UDP listener on port 8888:\n{ex.Message}");
            }
        }

        private async Task ReceiveDataAsync()
        {
            if (_udpClient == null) return;

            while (true)
            {
                try
                {
                    var result = await _udpClient.ReceiveAsync();
                    string json = Encoding.UTF8.GetString(result.Buffer);

                    using (JsonDocument doc = JsonDocument.Parse(json))
                    {
                        var root = doc.RootElement;
                        Dispatcher.Invoke(() =>
                        {
                            foreach (var property in root.EnumerateObject())
                            {
                                string key = property.Name;
                                string valueStr;

                                if (property.Value.ValueKind == JsonValueKind.Number && property.Value.TryGetDouble(out double dVal))
                                {
                                    valueStr = dVal.ToString("0.00");
                                    
                                    // グラフ用の履歴に追加
                                    if (key.Equals("System/FPS", StringComparison.OrdinalIgnoreCase))
                                        AddHistory(_fpsHistory, dVal);
                                    else if (key.Equals("System/CPU_Time_ms", StringComparison.OrdinalIgnoreCase))
                                        AddHistory(_cpuHistory, dVal);
                                    else if (key.Equals("System/GPU_Time_ms", StringComparison.OrdinalIgnoreCase))
                                        AddHistory(_gpuHistory, dVal);
                                }
                                else if (property.Value.ValueKind == JsonValueKind.Array)
                                {
                                    valueStr = string.Join(", ", property.Value.EnumerateArray().Select(e => e.ToString()));
                                }
                                else
                                {
                                    valueStr = property.Value.ToString();
                                }

                                var existing = _metrics.FirstOrDefault(m => m.Key == key);
                                if (existing != null)
                                {
                                    existing.Value = valueStr;
                                }
                                else
                                {
                                    _metrics.Add(new MetricItem { Key = key, Value = valueStr });
                                }
                            }

                            // フレームの最後にグラフを更新
                            UpdateGraph();
                        });
                    }
                }
                catch (Exception)
                {
                    // Ignore parse errors or socket closures
                }
            }
        }

        private void AddHistory(Queue<double> queue, double value)
        {
            queue.Enqueue(value);
            if (queue.Count > MaxHistory) queue.Dequeue();
        }

        private void UpdateGraph()
        {
            double width = GraphCanvas.ActualWidth;
            double height = GraphCanvas.ActualHeight;
            if (width <= 0 || height <= 0) return;

            // スケール最大値（FPSは120、CPU/GPU時間はとりあえず33.3ms = 約30FPS基準）
            UpdatePolylineAndPolygon(FpsLine, FpsArea, _fpsHistory, width, height, 120.0);
            UpdatePolylineAndPolygon(CpuLine, CpuArea, _cpuHistory, width, height, 33.3);
            UpdatePolylineAndPolygon(GpuLine, GpuArea, _gpuHistory, width, height, 33.3);
        }

        private void UpdatePolylineAndPolygon(System.Windows.Shapes.Polyline polyline, System.Windows.Shapes.Polygon polygon, Queue<double> history, double width, double height, double maxValue)
        {
            var points = new PointCollection();
            int count = history.Count;
            if (count == 0) return;

            double step = width / (MaxHistory - 1);
            double startX = width - (count * step);

            int i = 0;
            foreach (var val in history)
            {
                double x = startX + (i * step);
                double normalized = Math.Min(val / maxValue, 1.0);
                normalized = Math.Max(normalized, 0.0);
                double y = height - (normalized * height);
                
                points.Add(new Point(x, y));
                i++;
            }
            
            // 折れ線用
            polyline.Points = points;
            
            // エリアグラフ（Polygon）用：始点と終点を下部(height)に落として閉じた図形にする
            var areaPoints = new PointCollection(points);
            if (points.Count > 0)
            {
                areaPoints.Add(new Point(points.Last().X, height)); // 最後の点から真下へ
                areaPoints.Add(new Point(points.First().X, height)); // 最初の点の真下へ
            }
            polygon.Points = areaPoints;
        }

        private void GraphCanvas_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            UpdateGraph();
        }

        #region Remote Control (TCP : 8889)

        private TcpClient? _cmdTcpClient;
        private NetworkStream? _cmdStream;
        private System.Threading.CancellationTokenSource? _cmdCts;

        private void StartCommandClient()
        {
            _cmdCts = new System.Threading.CancellationTokenSource();
            Task.Run(() => CommandClientLoopAsync(_cmdCts.Token));
        }

        private async Task CommandClientLoopAsync(System.Threading.CancellationToken ct)
        {
            while (!ct.IsCancellationRequested)
            {
                try
                {
                    Dispatcher.Invoke(() =>
                    {
                        ConnectionLed.Fill = new SolidColorBrush(Color.FromRgb(102, 112, 133)); // Gray
                        CommandStatusText.Text = "Connecting to Game (127.0.0.1:8889)...";
                        CommandStatusText.Foreground = new SolidColorBrush(Color.FromRgb(138, 146, 178));
                    });

                    _cmdTcpClient = new TcpClient();
                    await _cmdTcpClient.ConnectAsync("127.0.0.1", 8889, ct);
                    _cmdStream = _cmdTcpClient.GetStream();

                    Dispatcher.Invoke(() =>
                    {
                        ConnectionLed.Fill = new SolidColorBrush(Color.FromRgb(0, 255, 157)); // Green
                        CommandStatusText.Text = "● Connected (TCP : 8889)";
                        CommandStatusText.Foreground = new SolidColorBrush(Color.FromRgb(0, 255, 157));
                    });

                    // サーバーからの実行結果レスポンスを受信するループ
                    byte[] buffer = new byte[4096];
                    StringBuilder sb = new StringBuilder();

                    while (!ct.IsCancellationRequested && _cmdTcpClient.Connected)
                    {
                        int bytesRead = await _cmdStream.ReadAsync(buffer, 0, buffer.Length, ct);
                        if (bytesRead <= 0) break; // 切断

                        string received = Encoding.UTF8.GetString(buffer, 0, bytesRead);
                        sb.Append(received);

                        string fullStr = sb.ToString();
                        int newlineIdx;
                        while ((newlineIdx = fullStr.IndexOf('\n')) >= 0)
                        {
                            string line = fullStr.Substring(0, newlineIdx).TrimEnd('\r');
                            fullStr = fullStr.Substring(newlineIdx + 1);

                            if (!string.IsNullOrWhiteSpace(line))
                            {
                                Dispatcher.Invoke(() =>
                                {
                                    CommandStatusText.Text = line;
                                    if (line.Contains("[Success]", StringComparison.OrdinalIgnoreCase))
                                    {
                                        CommandStatusText.Foreground = new SolidColorBrush(Color.FromRgb(0, 255, 157)); // Green
                                    }
                                    else if (line.Contains("[Error]", StringComparison.OrdinalIgnoreCase) || line.Contains("Unknown", StringComparison.OrdinalIgnoreCase))
                                    {
                                        CommandStatusText.Foreground = new SolidColorBrush(Color.FromRgb(255, 51, 102)); // Red
                                    }
                                    else
                                    {
                                        CommandStatusText.Foreground = new SolidColorBrush(Color.FromRgb(0, 229, 255)); // Cyan
                                    }
                                });
                            }
                        }
                        sb.Clear();
                        sb.Append(fullStr);
                    }
                }
                catch (Exception)
                {
                    // 接続失敗またはエラー時は少し待ってリトライ
                }
                finally
                {
                    _cmdStream?.Dispose();
                    _cmdStream = null;
                    _cmdTcpClient?.Close();
                    _cmdTcpClient = null;

                    Dispatcher.Invoke(() =>
                    {
                        ConnectionLed.Fill = new SolidColorBrush(Color.FromRgb(102, 112, 133)); // Gray
                        CommandStatusText.Text = "Disconnected. Retrying connection...";
                        CommandStatusText.Foreground = new SolidColorBrush(Color.FromRgb(138, 146, 178));
                    });
                }

                try
                {
                    await Task.Delay(1500, ct);
                }
                catch (OperationCanceledException)
                {
                    break;
                }
            }
        }

        public async void SendCommand(string command)
        {
            if (string.IsNullOrWhiteSpace(command)) return;

            if (_cmdStream == null || _cmdTcpClient == null || !_cmdTcpClient.Connected)
            {
                CommandStatusText.Text = "Cannot send: Not connected to Game.";
                CommandStatusText.Foreground = new SolidColorBrush(Color.FromRgb(255, 51, 102)); // Red
                return;
            }

            try
            {
                byte[] data = Encoding.UTF8.GetBytes(command + "\n");
                await _cmdStream.WriteAsync(data, 0, data.Length);
                CommandStatusText.Text = $"Sent: \"{command}\" (Waiting response...)";
                CommandStatusText.Foreground = new SolidColorBrush(Color.FromRgb(0, 229, 255)); // Cyan
            }
            catch (Exception ex)
            {
                CommandStatusText.Text = $"Send Error: {ex.Message}";
                CommandStatusText.Foreground = new SolidColorBrush(Color.FromRgb(255, 51, 102)); // Red
            }
        }

        private void Button_Spawn10k_Click(object sender, RoutedEventArgs e)
        {
            SendCommand("spawn_debris 10000");
        }

        private void Button_Spawn1k_Click(object sender, RoutedEventArgs e)
        {
            SendCommand("spawn_debris 1000");
        }

        private void Button_ClearDebris_Click(object sender, RoutedEventArgs e)
        {
            SendCommand("clear_debris");
        }

        private void SendCommandButton_Click(object sender, RoutedEventArgs e)
        {
            string cmd = CommandInputTextBox.Text.Trim();
            if (!string.IsNullOrEmpty(cmd))
            {
                SendCommand(cmd);
                CommandInputTextBox.Clear();
            }
        }

        private void CommandInputTextBox_KeyDown(object sender, System.Windows.Input.KeyEventArgs e)
        {
            if (e.Key == System.Windows.Input.Key.Enter)
            {
                string cmd = CommandInputTextBox.Text.Trim();
                if (!string.IsNullOrEmpty(cmd))
                {
                    SendCommand(cmd);
                    CommandInputTextBox.Clear();
                }
            }
        }

        #endregion

        protected override void OnClosed(EventArgs e)
        {
            _cmdCts?.Cancel();
            _cmdStream?.Dispose();
            _cmdTcpClient?.Close();
            _udpClient?.Close();
            base.OnClosed(e);
        }
    }
}