# RTSP_ns-3

### HOWTO
ns-3.xx 디렉토리에서 아래 명령어 실행
실행 전에 ns-3.xx/scratch 디렉토리가 비어 있어야 함

```bash
$ git clone https://github.com/again7536/RTSP_ns-3.git temp
$ mv temp/.git .git
$ rm -rf temp
$ git pull

$ ./waf clean
$ ./waf configure --enable-examples --enable-tests --build-profile=debug
$ ./waf
```

테스트 파일 실행
```bash
$ ./waf --run RtspTest
```

### 새로운 파일 생성
.gitignore에 명시적으로 추가해야 합니다
