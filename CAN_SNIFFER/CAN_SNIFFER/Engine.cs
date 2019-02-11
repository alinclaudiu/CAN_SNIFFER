using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace CAN_SNIFFER
{
    public class Engine
    {
        public static List<int> AvailableCOMBaudRates;
        public static string SelectedCOMPort = "";
        public static int SelectedCOMBaudRate = -1;

        private static List<CAN_Message> allCANMessages;
        private static List<CAN_Message> filteredCANMessages;
        private static int FILTER_MAXCOUNT = -1;
        private static int FILTER_MAXDATACHANGES = -1;
        private static bool FILTERS_ACTIVE = false;

        static COMData comListener;
        static SerialPort serialPort;

        public static void Initialize()
        {
            AvailableCOMBaudRates = new List<int>();
            AvailableCOMBaudRates.Add(300);
            AvailableCOMBaudRates.Add(1200);
            AvailableCOMBaudRates.Add(2400);
            AvailableCOMBaudRates.Add(4800);
            AvailableCOMBaudRates.Add(9600);
            AvailableCOMBaudRates.Add(19200);
            AvailableCOMBaudRates.Add(38400);
            AvailableCOMBaudRates.Add(57600);
            AvailableCOMBaudRates.Add(74880);
            AvailableCOMBaudRates.Add(115200);
            AvailableCOMBaudRates.Add(230400);
            AvailableCOMBaudRates.Add(250000);
            AvailableCOMBaudRates.Add(500000);
            AvailableCOMBaudRates.Add(1000000);
            AvailableCOMBaudRates.Add(2000000);

            allCANMessages = new List<CAN_Message>();
            filteredCANMessages = new List<CAN_Message>();
        }

        public static int StartCOM(COMData listener)
        {           
            if (SelectedCOMBaudRate == -1 || SelectedCOMPort == "")
            {
                return 1;                    
            }
            comListener = listener;

            try
            {
                serialPort = new SerialPort(SelectedCOMPort);
                serialPort.BaudRate = SelectedCOMBaudRate;
                serialPort.Open();
                serialPort.DataReceived += SerialPort_DataReceived;                
            }
            catch (Exception e)
            {
                return 2;
            }

            return 0;
        }      
        
        public static void SendCOM(string cmd)
        {
            if (serialPort.IsOpen)
            {
                serialPort.WriteLine(cmd);
            }
        }

        public static void StopCOM()
        {
            Thread endingThread = new Thread(new ThreadStart(CloseSerialPort));
            endingThread.Start();
        }

        private static void CloseSerialPort()
        {
            if (serialPort != null)
            {
                serialPort.Close();
            }
        }

        private static void SerialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                string str = serialPort.ReadLine();
                Debug.WriteLine(str);
                handleReceivedData(str);
            }
            catch (Exception exc)
            {
                comListener.CAN_Receive_Error(exc.Message);
            }            
        }

        private static void handleReceivedData(string data)
        {
            if (data.StartsWith("##START##"))
            {
                if (data.EndsWith("##END##"))
                {
                    try
                    {
                        data = data.Replace("##START##", "");
                        data = data.Replace("##END##", "");

                        string[] parts = new string[10];
                        parts = data.Split('%');

                        CAN_Message canMessage = new CAN_Message();

                        for (int i = 0; i < parts.Length; i++)
                        {
                            if (i == 0) { canMessage.ID = parts[i]; }      // ID
                            if (i == 1) { canMessage.DLC = parts[i]; }     // DLC
                            if (i > 1)                                     // CAN Data
                            {
                                canMessage.canData[i - 2] = parts[i];
                            }
                        }

                        //comListener.CAN_Message_Received(canMessage);
                        AddToAllMessagesList(canMessage);
                    }
                    catch (Exception ex)
                    {
                        comListener.CAN_Receive_Error(ex.Message);                      
                    }
                }
            }           
            
            if (data.StartsWith("##CanSPEED##"))
            {
                data = data.Replace("##CanSPEED##", "");
                data = data.Remove(data.LastIndexOf("kbps") + 4);
                comListener.CAN_CanSpeed_Received(data);
            }
        }

        private static void AddToAllMessagesList(CAN_Message msg)
        {
            bool messageAlreadyThere = false;
            foreach (CAN_Message temp in allCANMessages)
            {
                if (msg.ID.Equals(temp.ID))
                {
                    temp.Count++;
                    temp.canData = msg.canData;
                    messageAlreadyThere = true;
                   
                    if (!Enumerable.SequenceEqual(temp.canData, msg.canData)) 
                    {
                        temp.DataChanges++;
                    }
                }
            }
            if (!messageAlreadyThere)
            {
                allCANMessages.Add(msg);
            }

            if (!FILTERS_ACTIVE)
            {
                comListener.CAN_MessageList_Update(allCANMessages);
            }
            else
            {
                CalculateFilteredMessageList();
                comListener.CAN_MessageList_Update(filteredCANMessages);
            }
        }

        public static void ClearAllMessages()
        {
            if (comListener != null)
            {
                allCANMessages.Clear();
                comListener.CAN_MessageList_Update(allCANMessages);
            }            
        }

        private static void CalculateFilteredMessageList()
        {
            filteredCANMessages.Clear();

            if (FILTER_MAXDATACHANGES == -1 || FILTER_MAXCOUNT == -1)
            {
                if (FILTER_MAXDATACHANGES == -1)
                {
                    foreach (CAN_Message msg in allCANMessages)
                    {
                        if (msg.Count <= FILTER_MAXCOUNT)
                        {
                            filteredCANMessages.Add(msg);
                        }
                    }
                }
                if (FILTER_MAXCOUNT == -1)
                {
                    foreach (CAN_Message msg in allCANMessages)
                    {
                        if (msg.DataChanges <= FILTER_MAXDATACHANGES)
                        {
                            filteredCANMessages.Add(msg);
                        }
                    }
                }
                if (FILTER_MAXDATACHANGES == -1 && FILTER_MAXCOUNT == -1)
                {
                    foreach (CAN_Message msg in allCANMessages)
                    {
                        filteredCANMessages.Add(msg);
                    }
                }
            }
            else
            {
                foreach (CAN_Message msg in allCANMessages)
                {
                    if (msg.DataChanges <= FILTER_MAXDATACHANGES && msg.Count <= FILTER_MAXCOUNT)
                    {
                        filteredCANMessages.Add(msg);
                    }
                }
            }                        
        }

        public static void ApplyFilter(int maxCount, int maxDataChanges)
        {
            FILTER_MAXCOUNT = maxCount;
            FILTER_MAXDATACHANGES = maxDataChanges;
            FILTERS_ACTIVE = true;
        }

        public static void UndoAllFilters()
        {
            FILTERS_ACTIVE = false;
            FILTER_MAXCOUNT = -1;
            FILTER_MAXDATACHANGES = -1;

            if (comListener != null)
            {
                comListener.CAN_MessageList_Update(allCANMessages);
            }
        }
    }

    public interface COMData
    {
        void CAN_Message_Received(CAN_Message msg);
        void CAN_MessageList_Update(List<CAN_Message> messages);
        void CAN_Receive_Error(string error);
        void CAN_CanSpeed_Received(string canSpeed);
    }
}
