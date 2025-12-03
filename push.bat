@echo off
chcp 65001
:: 한글 깨짐 방지를 위한 명령어입니다.

echo [1/3] 변경된 모든 파일을 리스트에 담습니다 (git add)
git add .

echo.
:: 여기서 사용자가 메시지를 입력하게 합니다.
set /p msg="커밋 메시지를 입력하세요 (예: 버그 수정): "

echo.
echo [2/3] 로컬에 저장합니다 (git commit)
git commit -m "%msg%"

echo.
echo [3/3] GitHub로 업로드합니다 (git push)
git push

echo.
echo 모든 작업이 완료되었습니다!
pause