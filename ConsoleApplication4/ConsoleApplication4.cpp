#include <conio.h>	// _kbhitで使用
#include <stdio.h>	// printfで使用

#include <pdh.h>	// Pdh関係のヘッダファイル
#include <pdhmsg.h>	// Pdhのリターンメッセージ関係のヘッダファイル

/*
*  Pdhの基本的な使い方
*/
int test() {
	PDH_HQUERY              hQuery;
	PDH_HCOUNTER            hCounter;
	PDH_FMT_COUNTERVALUE    fmtValue;
	PDH_STATUS				status;

	/*1 クエリを作成する*/
	status = PdhOpenQuery(NULL, 0, &hQuery);
	if (status != ERROR_SUCCESS) {
		return 1;
	}

	/*
	* 2 クエリにカウンターを追加する
	* この例ではパフォーマンスカウンターでの「\Processor(_Total)\% Processor Time」を追加している
	*/
	status = PdhAddCounter(hQuery, TEXT("\\Processor(_Total)\\% Processor Time"), 0, &hCounter);
	if (status != ERROR_SUCCESS) {
		PdhCloseQuery(hQuery);
		return 1;
	}

	/*
	* 3 パフォーマンスデータを収集する
	* 3-1 多くのカウンターではデータ値を計算するために2つのサンプルデータが必要とのこと
	* そのため、最初に一度サンプルデータの収集を行う
	*/
	status = PdhCollectQueryData(hQuery);
	PdhGetFormattedCounterValue(hCounter, PDH_FMT_DOUBLE, NULL, &fmtValue);
	if (status != ERROR_SUCCESS) {
		PdhCloseQuery(hQuery);
		return 1;
	}

	/* キーが押されるまでループ*/
	while (!_kbhit()) {
		/*3-2 コレクション間で少なくとも１秒以上待機する*/
		Sleep(1000);
		system("CLS");

		/*3-3 再度サンプルデータを収集する*/
		PdhCollectQueryData(hQuery);

		/*
		* 4 サンプル取得後、表示可能な値を計算する
		* この例ではDOUBLEがたに変換して表示
		*/
		PdhGetFormattedCounterValue(hCounter, PDH_FMT_DOUBLE, NULL, &fmtValue);
		printf("doubleValue = %.15f\n", fmtValue.doubleValue);
	}

	/*
	* 5 クエリのエータ収集が完了したら、けりを閉じてシステムリソースを開放する
	* クエリに関連付けられているすべてのカウンターハンドルを閉じる
	*/
	PdhCloseQuery(hQuery);
	return 0;
}

/*
* PdhExpandWildCardPathとPdhGetCounterInfoを使い、
* 特定のカウンターパスをすべて取得、表示する
*/
int test2() {
	PDH_HQUERY              hQuery;
	HCOUNTER				hCounter[512];
	PDH_FMT_COUNTERVALUE	fmtValue[512];
	PDH_STATUS				status;

	status = PdhOpenQuery(NULL, 0, &hQuery);
	if (status != ERROR_SUCCESS) {
		return 1;
	}

	/*
	* 1 ワイルドカードを含むカウンターパス
	* ここでは\Process(*)\% Processor Timeとする。
	*/
	LPCWSTR					wildCardPath = TEXT("\\Process(*)\\% Processor Time");
	unsigned long			pathListLength = 0;
	/*
	* 2-1 PdhExpandWildCardPathは2回実行しなければならない。
	* 1回目は取得するカウンターパスリストのサイズを取得する。
	* ちなみに、カウンターパスリストを格納する変数はない（NULL）ので、
	* 正常に動作していれば、statusはPDH_MORE_DATAが返ってくるはず。
	*/
	status = PdhExpandWildCardPath(NULL, wildCardPath, NULL, &pathListLength, 0);
	if (status != ERROR_SUCCESS && status != PDH_MORE_DATA) {
		PdhCloseQuery(hQuery);
		return 1;
	}

	/*
	* カウンターパスリストのサイズがunsigned longの最大値となっていないかを確認する。
	* 最大値の場合、次のメモリ確保の時にバッファオーバフローが発生するため、処理を終了する。
	* （おそらく、そんなことはないとは思うが）
	*/
	if (pathListLength == ULONG_MAX) {
		PdhCloseQuery(hQuery);
		return 1;
	}

	/*
	* カウンターパスリストのサイズ + 1の分だけ、メモリを確保する。
	* カウンターパスは最後はNULL文字で終わっていないといけないので、その分が増えている。
	*/
	TCHAR* expandedPathList = (TCHAR*)calloc(pathListLength + 1, sizeof(TCHAR));
	/* 2-2 PdhExpandWildCardPathを再度実行し、expandedPathListにデータを格納する*/
	PdhExpandWildCardPath(NULL, wildCardPath, expandedPathList, &pathListLength, 0);


	TCHAR* epl = expandedPathList;
	int		pathListNum = 0;

	if (epl == NULL) {
		return 0;
	}

	while (*epl) {
		/* 終端がNULL文字の取得したカウンターパスリストを一つ追加する*/
		PdhAddCounter(hQuery, epl, 0, &hCounter[pathListNum]);
		pathListNum++;
		/* 次のカウンターパスが格納されているアドレスに移動する*/
		epl += wcslen(epl) + 1;
	}

	free(expandedPathList);

	status = PdhCollectQueryData(hQuery);
	if (status != ERROR_SUCCESS) {
		PdhCloseQuery(hQuery);
		return 1;
	}

	while (!_kbhit()) {
		Sleep(1000);
		PdhCollectQueryData(hQuery);
		/* カウンターパスの現在値を一つずつ取得する*/
		for (int i = 0; i < pathListNum; i++) {
			unsigned long		bufferSize = 0;
			/*
			* 3 PdhGetCounterInfoを実行して、現在のカウンターの情報を取得する。
			* PdhExpandWildCardPathと同様、2回実行しなければならない。
			*/
			status = PdhGetCounterInfo(hCounter[i], TRUE, &bufferSize, NULL);
			if (status != ERROR_SUCCESS && status != PDH_MORE_DATA) {
				PdhCloseQuery(hQuery);
				return 1;
			}
			PPDH_COUNTER_INFO	pdhConterInfo = (PPDH_COUNTER_INFO)calloc(bufferSize, sizeof(PPDH_COUNTER_INFO));
			PdhGetCounterInfo(hCounter[i], TRUE, &bufferSize, pdhConterInfo);

			if (pdhConterInfo == nullptr) {
				continue;
			}
			printf("%ls ", pdhConterInfo->szFullPath);

			free(pdhConterInfo);

			PdhGetFormattedCounterValue(hCounter[i], PDH_FMT_DOUBLE, NULL, &fmtValue[i]);
			printf("doubleValue = %.15f\n", fmtValue[i].doubleValue);
		}
	}

	PdhCloseQuery(hQuery);
	return 0;
}

int main()
{
	// test();
	test2();
}