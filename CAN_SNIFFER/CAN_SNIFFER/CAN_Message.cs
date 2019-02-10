using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CAN_SNIFFER
{
    public class CAN_Message
    {
        public string ID;
        public string DLC;
        public string[] canData;
        public int Count;

        public CAN_Message()
        {
            canData = new string[8];
            Count = 1;
        }

        public CAN_Message(string id, string dlc, string[] data)
        {
            ID = id;
            DLC = dlc;
            canData = data;
            Count = 1;
        }
    }
}
