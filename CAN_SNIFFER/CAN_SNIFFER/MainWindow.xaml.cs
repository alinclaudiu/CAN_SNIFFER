using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.IO.Ports;

namespace CAN_SNIFFER
{
  
    public partial class MainWindow : Window, COMData
    {
        ScrollViewer scrollViewer_listViewData;

        public MainWindow()
        {
            InitializeComponent();
            Engine.Initialize();

            foreach (string port in SerialPort.GetPortNames())
            {
                if (!port.Equals(""))
                {
                    comboBox_COMPorts.Items.Add(port);
                }
            }

            foreach (int baud in Engine.AvailableCOMBaudRates)
            {
                comboBox_baudRate.Items.Add(baud);
            }                        
        }
        
        private void button_startStop_Click(object sender, RoutedEventArgs e)
        {
            Button button = (Button)sender;
            string content = (String)button.Content;
            if (content.Equals("Start"))
            {
                int result = Engine.StartCOM(this);

                if (result == 0)
                {
                    button_requestCanSpeed.IsEnabled = true;
                    button.Content = "Stop";
                    button.Background = Brushes.Red;
                }
                else if (result == 1)
                {
                    MessageBox.Show("Please select COM Baud rate and COM Port!", "Attention", MessageBoxButton.OK, MessageBoxImage.Information);
                }
                else if (result == 2)
                {
                    MessageBox.Show("Could not connect on SerialPort!", "ERROR", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
            if (content.Equals("Stop"))
            {
                Engine.StopCOM();

                button.Content = "Start";
                button_requestCanSpeed.IsEnabled = false;
                label_canSpeed.Content = "-----";
                button.Background = Brushes.Green;
            }
        }       

        private void comboBox_COMPorts_selectionChanged(object sender, SelectionChangedEventArgs e)
        {
            Engine.SelectedCOMPort = (string)comboBox_COMPorts.SelectedItem;
        }

        private void comboBox_baudRate_selectionChanged(object sender, SelectionChangedEventArgs e)
        {
            Engine.SelectedCOMBaudRate = (int)comboBox_baudRate.SelectedItem;
        }

        public void CAN_Message_Received(CAN_Message msg)
        {
            listView_data.Dispatcher.Invoke(new Action(() => {
                listView_data.Items.Add(new ListViewItemCANMsg
                {
                    ID = msg.ID,
                    DLC = msg.DLC,
                    Bit1 = msg.canData[0],
                    Bit2 = msg.canData[1],
                    Bit3 = msg.canData[2],
                    Bit4 = msg.canData[3],
                    Bit5 = msg.canData[4],
                    Bit6 = msg.canData[5],
                    Bit7 = msg.canData[6],
                    Bit8 = msg.canData[7],
                    Count = msg.Count.ToString()
                });                
            }));            
        }

        public void CAN_Receive_Error(string error)
        {
            MessageBox.Show(error, "ERROR", MessageBoxButton.OK, MessageBoxImage.Error);
        }

        public void CAN_MessageList_Update(List<CAN_Message> messages)
        {
            listView_data.Dispatcher.Invoke(new Action(() => {
                listView_data.Items.Clear();
                foreach (CAN_Message msg in messages)
                {
                    listView_data.Items.Add(new ListViewItemCANMsg
                    {
                        ID = msg.ID,
                        DLC = msg.DLC,
                        Bit1 = msg.canData[0],
                        Bit2 = msg.canData[1],
                        Bit3 = msg.canData[2],
                        Bit4 = msg.canData[3],
                        Bit5 = msg.canData[4],
                        Bit6 = msg.canData[5],
                        Bit7 = msg.canData[6],
                        Bit8 = msg.canData[7],
                        Count = msg.Count.ToString()
                    });
                }
            }));
        }

        public void CAN_CanSpeed_Received(string canSpeed)
        {
            label_canSpeed.Dispatcher.Invoke(new Action(() => {
                label_canSpeed.Content = canSpeed;
            }));
        }

        private void button_refreshCOMPorts_Click(object sender, RoutedEventArgs e)
        {
            comboBox_COMPorts.Items.Clear();
            foreach (string port in SerialPort.GetPortNames())
            {
                if (!port.Equals(""))
                {
                    comboBox_COMPorts.Items.Add(port);
                }
            }
        }

        private void button_requestCanSpeed_Click(object sender, RoutedEventArgs e)
        {
            label_canSpeed.Content = "loading...";
            Engine.SendCOM("requestCANSpeed");
        }

        private void button_ClearListViewData_Click(object sender, RoutedEventArgs e)
        {
            Engine.ClearAllMessages();
        }
    }
}
