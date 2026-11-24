# 플레이 테스트 수정사항 검증표

기준 빌드: TroubleSquad_VFX_20261119_204446 (2026-11-19). 사용자의 테스트 우선 요청에 따라 패키징했다. 전체 목표 100% 완료 판정은 하지 않는다.

| 요청 | 현재 근거 | 남은 확인 |
|---|---|---|
| 로비의 흰 맵 이미지 수정 | 직접 에셋 경로 수정, 4맵 선택창 렌더 캡처 | 친구 PC에서 표시 확인 |
| 캐릭터/맵 클릭 선택창과 업그레이드 아이콘 | FeedbackReview의 MapSelection, ChampionSelection, Upgrades 캡처 | 다양한 해상도에서 사용성 |
| 4맵 구조·배치·디자인 | 배치/Blender 구조물 추가, 최신 3맵 개요 렌더 | 원작 대비 구조·표면·장식 완성도 미달. 별도 개선 필요 |
| 실제 맵과 미니맵 일치 | 실제 지형 SceneCapture와 동일 좌표 변환 | 모든 보스 전환 중 표시와 다인 마커 확인 |
| 9캐릭터 크기·조준 방향 | PlayerFeedback sizes=1 directions=1 | 실제 플레이에서 포즈별 확인 |
| E/R 쿨감·6레벨 궁극기 | PlayerFeedback haste=1 ultimateLocked=1 ultimateUnlocked=1, Cooldown0..8 통과 | 실제 선택 카드와 HUD 체감 확인 |
| Esc 설정과 상세 화면/음량 설정 | Settings acceptance 7개 플래그 통과, 최신 Editor/Game 빌드 | 친구 PC 해상도·전체 화면, 저장/취소 조작 |
| 몬스터 기립·이동 | EnemyMotion upright=1 facing=1 moving=1 animated=1, 25종 rig/footing 검사 | 모든 종의 전투 포즈·접지 |
| 몬스터 생성량 | Density batch=3 population=80 bounded=1, 후반 cap=180 | 장시간 XP 성장 속도와 저사양 성능 |
| 스킬/무기 가독성 | Blender 형상 6종 로드, 20무기 갤러리, 집중 VFX 7건 통과 | 모든9캐릭터 E/R·전용무기 및 20무기 실전 재검수 |
| 몬스터 피해 숫자·스킬 설명 | 실제 피해 기반 복제/표시와 hover 텍스트 구현, PlayerFeedback damage=1 | 전체 스킬 툴팁 수치와 화면 배치 실전 확인 |
| 테스트용 패키지 전달 | 공유폴더 69파일 1,451,670,624바이트, 해시 대조 | 사용자 친구 테스트 |

## 실행 근거

- Saved/Logs/CurrentFeedbackRegression.log: 27 cases 통과, 사용자 저장 파일 해시 유지.
- Saved/Logs/PlaytestFinalEditorBuild.log: 최신 Editor 빌드 성공.
- Saved/Logs/FriendFeedbackPackaging.log: Windows 패키징 성공, 로비와4맵 시작 통과.
- Saved/Logs/FriendFeedbackCoop.log: 로컬 방장+3클라이언트 참가 및 서로 다른 캐릭터 선택 통과. Steam 원격 전송·이동·스킬·완주 검증을 대신하지 않는다.
- Saved/Logs/FriendFeedbackShare.log 및 VfxShareVerification.json: 공유 복사 검증.
- Docs/PlaytestReleaseManifest.json: 전달본 전체 파일의 SHA-256. 전달 후 소스 수정과 혼동하지 않도록 보관한다.

공유본은 고정했다. 커밋·푸시는 수행하지 않았다. 전체 목표의 미완료 항목은 이 표에 남긴다.
