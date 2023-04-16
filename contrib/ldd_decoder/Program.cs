using System;
using Dchecker.Models;
using System.IO;
using System.Text;
using System;
using System.Collections;
using System.Collections.Generic;

namespace ldd_decoder
{
    class Program
    {
        static void Main(string[] args)
        {
            // Decode binary LDD
            Console.WriteLine("Decoding LDD file...");
            LabelDefine[] labeldefines = DataConvModel.DeserializeLabelDefine(
                "D:\\Dchecker3_V3602\\Dchecker3\\Storage\\DataModule\\Conv\\S\\en\\DEFAULT.ldd");

            // Export it to CSV
            Console.WriteLine("Exporting CSV...");
            string outDir = Directory.GetCurrentDirectory();
            bool res = new DataConvModel(new SamplingInfo()).SaveLabelDefine(
                "label_defines.csv", labeldefines, false, outDir);

            // Format corresponding ESPAltherma def include file
            Console.WriteLine("Generating ESPAltherma def include file...");
            GenerateDefInclude("label_defines.h", labeldefines, outDir);

            Console.WriteLine("Done.");
        }

        static void GenerateDefInclude(string fileName, LabelDefine[] defines, string folder = null)
        {
            StringBuilder stringBuilder = new StringBuilder(
                "#include \"labeldef.h\"\n// Generated def file\nLabelDef labelDefs[] = {\n");

            foreach (LabelDefine define in defines)
            {
                // {0x50,0,217,1,-1,"Operation Mode"},
                stringBuilder.Append(String.Format("{{0x{0:X},{1},{2},{3},-1,\"{4}\"}},\n", Encoding.Default.GetBytes(define.DataId)[0], define.Offset, define.ConvId, define.Bytes, define.LabelStr));
                

                //stringBuilder.Append(Environment.NewLine);
            }
            stringBuilder.Append("};\n");
            using (StreamWriter streamWriter = new StreamWriter(folder + "\\" + fileName, false, Encoding.GetEncoding("utf-8")))
            {
                streamWriter.Write((object)stringBuilder);
                streamWriter.Close();
            }
        }
    }
}
