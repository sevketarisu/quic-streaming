package com.github.sevketarisu.quicstreaming;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;

public class CsvMerger {

	static String FILE_WALKER_FOLDER;

	public static void main(String[] args) {

		// TODO Auto-generated method stub

		FILE_WALKER_FOLDER = args[0];
		ArrayList<String> quicFileList = new ArrayList<>();
		ArrayList<String> curlFileList = new ArrayList<>();
		ArrayList<String> urllibFileList = new ArrayList<>();
		Filewalker fw = new Filewalker();
		try {
			fw.walk(FILE_WALKER_FOLDER, quicFileList, curlFileList,urllibFileList);
			mergeFiles(quicFileList, "QUIC", FILE_WALKER_FOLDER);
			mergeFiles(curlFileList, "CURL", FILE_WALKER_FOLDER);
			mergeFiles(urllibFileList, "URLLIB", FILE_WALKER_FOLDER);
		} catch (Exception e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}

	}

	public static void mergeFiles(ArrayList<String> fileList, String type,
			String folder) throws IOException {

		FileReader fileReader = null;
		FileWriter fileWriter = null;

		File out = new File(folder + File.separator + "MERGED_" + type + ".csv");
		fileWriter = new FileWriter(out);
		BufferedWriter bufferedWriter = new BufferedWriter(fileWriter);

		StringBuffer stringBuffer = new StringBuffer();
		boolean epochPassed = false;

		for (String filePath : fileList) {

			File in = new File(filePath);
			fileReader = new FileReader(in);
			BufferedReader bufferedReader = new BufferedReader(fileReader);

			String line;
			while ((line = bufferedReader.readLine()) != null) {
				if (line.startsWith("EpochTime") && (!epochPassed)) {
					stringBuffer.append(line);
					stringBuffer.append("\n");
					epochPassed = true;
				} else if (!line.startsWith("EpochTime")) {
					stringBuffer.append(line);
					stringBuffer.append("\n");
				}
			}

			fileReader.close();
		}
		bufferedWriter.write(stringBuffer.toString());
		bufferedWriter.flush();
		fileWriter.close();
	}

}
