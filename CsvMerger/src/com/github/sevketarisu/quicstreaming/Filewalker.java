package com.github.sevketarisu.quicstreaming;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.ArrayList;

public class Filewalker {

	public void walk(String path, ArrayList<String> quicFileList,
			ArrayList<String> curlFileList, ArrayList<String> urllibFileList)
			throws Exception {

		File root = new File(path);
		File[] list = root.listFiles();

		if (list == null)
			return;

		for (File f : list) {
			if (f.isDirectory()) {
				walk(f.getAbsolutePath(), quicFileList, curlFileList,
						urllibFileList);
			} else {
				if (f.getName().endsWith(".csv")
						& f.getName().startsWith("DASH_BUFFER_LOG")) {
					if (f.getName().contains("QUIC")) {
						System.out.println("File:" + f.getName());
						quicFileList.add(f.getAbsolutePath());
					} else if (f.getName().contains("CURL")) {
						System.out.println("File:" + f.getName());
						curlFileList.add(f.getAbsolutePath());
					} else if (f.getName().contains("URLLIB")) {
						System.out.println("File:" + f.getName());
						urllibFileList.add(f.getAbsolutePath());
					}
				}
			}
		}
	}

}