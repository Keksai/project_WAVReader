#include <QCoreApplication>
#include <QtEndian>
#include <QDebug>
#include <QFile>
#include <QDataStream>
#include <cmath>

//Wav Header
struct wav_header_t
{
    char chunkId[4]; //"RIFF" = 0x46464952
    quint32 chunkSize; //28
    char format[4]; //"WAVE" = 0x45564157
    char subchunk1ID[4]; //"fmt " = 0x20746D66
    quint32 subchunk1Size; //16
    quint16 audioFormat;
    quint16 numChannels;
    quint32 sampleRate;
    quint32 byteRate;
    quint16 blockAlign;
    quint16 bitsPerSample;
    //[quint16 wExtraFormatBytes;]
    //[Extra format bytes]
};

wav_header_t wavHeader;

void ReadWav (const QString fileName, const QString fileToSave)
{
    if (fileName != "") {
        QFile wavFile(fileName);
        if (!wavFile.open(QIODevice::ReadOnly))
        {
            qDebug() << "Error: Could not open file!";
            return;
        }

        //Read WAV file header
        QDataStream analyzeHeader (&wavFile);
        analyzeHeader.setByteOrder(QDataStream::LittleEndian);
        analyzeHeader.readRawData(wavHeader.chunkId, 4); // "RIFF"
        analyzeHeader >> wavHeader.chunkSize; // File Size
        analyzeHeader.readRawData(wavHeader.format,4); // "WAVE"
        analyzeHeader.readRawData(wavHeader.subchunk1ID,4); // "fmt"
        analyzeHeader >> wavHeader.subchunk1Size; // Format length
        analyzeHeader >> wavHeader.audioFormat; // Format type
        analyzeHeader >> wavHeader.numChannels; // Number of channels
        analyzeHeader >> wavHeader.sampleRate; // Sample rate
        analyzeHeader >> wavHeader.byteRate; // (Sample Rate * BitsPerSample * Channels) / 8
        analyzeHeader >> wavHeader.blockAlign; // (BitsPerSample * Channels) / 8.1
        analyzeHeader >> wavHeader.bitsPerSample; // Bits per sample

        //Print WAV header
        qDebug() << "File Size: " << wavHeader.chunkSize;
        qDebug() << "Format Length: " << wavHeader.subchunk1Size;
        qDebug() << "Format Type: " << wavHeader.audioFormat;
        qDebug() << "Number of Channels: " << wavHeader.numChannels;
        qDebug() << "Sample Rate: " <<  wavHeader.sampleRate;
        qDebug() << "Sample Rate * Bits/Sample * Channels / 8: " << wavHeader.byteRate;
        qDebug() << "Bits per Sample * Channels / 8.1: " << wavHeader.blockAlign;
        qDebug() << "Bits per Sample: " << wavHeader.bitsPerSample;

        // szukam kawalkow
        quint32 chunkDataSize = 0;
        QByteArray temp_buff;
        char buff[0x04];
        while (true)
        {
            QByteArray tmp = wavFile.read(0x04);
            temp_buff.append(tmp);
            int idx = temp_buff.indexOf("data");
            if (idx >= 0)
            {
                int lenOfData = temp_buff.length() - (idx + 4);
                memcpy(buff, temp_buff.constData() + idx + 4, lenOfData);
                int bytesToRead = 4 - lenOfData;
                // finish readind size of chunk
                if (bytesToRead > 0)
                {
                    int read = wavFile.read(buff + lenOfData, bytesToRead);
                    if (bytesToRead != read)
                    {
                        qDebug() << "Error: Something awful happens!";
                        return;
                    }
                }
                chunkDataSize = qFromLittleEndian<quint32>((const uchar*)buff); // zmiana kolejności bajtów
                break;
            }
            if (temp_buff.length() >= 8)
            {
                temp_buff.remove(0, 0x04);
            }
        }
        if (!chunkDataSize)
        {
            qDebug() << "Error: Chunk data not found!";
            return;
        }

        //Reading data from the file
        int samples = 0;
        QFile saveFile(fileToSave);
        saveFile.open(QIODevice::WriteOnly);

        quint32 occurenceNumberRight[65536];
        quint32 occurenceNumberMinusRight[65536];

        quint32 occurenceNumberLeft[65536];
        quint32 occurenceNumberMinusLeft[65536];


        double probabilitiesleft[65536];
        double probabilitiesMinusleft[65536];

        double probabilitiesright[65536];
        double probabilitiesMinusright[65536];

        if (wavHeader.numChannels == 2)
        {
            saveFile.write("L\t R\r\n");
        }
        while (wavFile.read(buff, 0x04) > 0)
        {
            chunkDataSize -= 4;
            ++samples;
            qint16 sampleChannel1 = qFromLittleEndian<qint16>(buff);
            qint16 sampleChannel2 = qFromLittleEndian<qint16>((buff + 2));

            if (sampleChannel1 > 0) {
                occurenceNumberLeft[sampleChannel1]++; //licznik wystapien
            }
            else {
                occurenceNumberMinusLeft[sampleChannel1*(-1)]++;
            }
            if (sampleChannel2 > 0) {
                occurenceNumberRight[sampleChannel2]++; //licznik wystapien
            }
            else {
                occurenceNumberMinusRight[sampleChannel2*(-1)]++;
            }

            // podzial kanalow wplywa na umeiszczenie
            switch (wavHeader.numChannels) {
            case 1:
                saveFile.write(QString("%1\r\n %2\r\n").arg(sampleChannel1).arg(sampleChannel2).toUtf8());
                break;
            case 2:
                saveFile.write(QString("%1\t %2\r\n").arg(sampleChannel1).arg(sampleChannel2).toUtf8());
                break;
            }
            // check the end of the file
            if (chunkDataSize == 0 || chunkDataSize & 0x80000000)
            {
                break;
            }
        }
        qDebug() << "Readed " << samples << " samples...";

        /* OBLICZANIE PRAWDOPODOBIENSTW */

        for (int j=0;j<65536;j++) {
            probabilitiesleft[j]=(double)occurenceNumberLeft[j]/samples;
            probabilitiesMinusleft[j]=(double)occurenceNumberMinusLeft[j]/samples;

            probabilitiesright[j]=(double)occurenceNumberRight[j]/samples;
            probabilitiesMinusright[j]=(double)occurenceNumberMinusRight[j]/samples;
        }

        /* SREDNIA */
        long double srednia = 0;
        long int iloscLiczb = 0;
        for (int j=0;j<65536;j++) {
            srednia = srednia + j*occurenceNumberLeft[j] + j*occurenceNumberRight[j];
            iloscLiczb = iloscLiczb +occurenceNumberLeft[j] + occurenceNumberRight[j];
            srednia = srednia + j*occurenceNumberMinusLeft[j] + j*occurenceNumberMinusRight[j];
            iloscLiczb = iloscLiczb +occurenceNumberMinusLeft[j] + occurenceNumberMinusRight[j];
        }
        srednia = (double)srednia/iloscLiczb;

        /* ENTROPIA */
        double entropiaL = 0;
        double entropiaR = 0;
        for (int j=1;j<65536;j++) {
            if(probabilitiesleft[j] != 0) // zgodnie z granica dla 0
                entropiaL += probabilitiesleft[j] * log2((1/probabilitiesleft[j]));
            if (probabilitiesMinusleft[j] != 0)
                entropiaL += probabilitiesMinusleft[j] * log2((1/probabilitiesMinusleft[j]));
            if (probabilitiesright[j] != 0)
                entropiaR += probabilitiesright[j] * log2((1/probabilitiesright[j]));
            if (probabilitiesMinusright[j] != 0)
                entropiaR += probabilitiesMinusright[j] * log2((1/probabilitiesMinusright[j]));
            if (j == 8009)

                qDebug() << "";
        }

        /* SUMA KWADRATOW L I P */

        long double powerSumLeft = 0;
        long double powerSumRight = 0;
        for (int j=1;j<65536;j++) {
            powerSumLeft += pow(static_cast<double>(occurenceNumberLeft[j]),2);
            powerSumLeft += pow(static_cast<double>(occurenceNumberMinusLeft[j]),2);
            powerSumRight += pow(static_cast<double>(occurenceNumberRight[j]),2);
            powerSumRight += pow(static_cast<double>(occurenceNumberMinusRight[j]),2);
        }

        /* SREDNIA KWADRATOWA Z SUMY KWADRATOW */

        /* TEST WARTOSCI */
        for (int i=990;i <1000;i++) {
            qDebug() << "Number: " << i
                     << " counted: " << occurenceNumberLeft[i]
                     <<" prob Lewy: "<< probabilitiesleft[i] // test wystapien
                     <<" prob Prawy: "<< probabilitiesright[i];
        }

        qDebug() << "srednia: " << (double)srednia << "ilosc: " << iloscLiczb;
        qDebug() << "entropia L: " << entropiaL << "entropia R: " << entropiaR;
        qDebug() << "Suma kwadratowa L: " << (double)powerSumLeft << "Suma kwadratowa R: " << (double)powerSumRight;


        wavFile.close();
        if (saveFile.isOpen())
        {
            saveFile.close();
        }
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    ReadWav("/home/isaac/project_WAVReader/chanchan.wav", "list.dat");
        return a.exec();
}
