# 공격 모션·사망음·Esc 추가 (2026-11-20)

공유본: C:/Users/cksdn/Desktop/공유폴더/TroubleSquad_VFX_20261120_001004

- Esc 설정 하단의 로비 나가기 버튼을 렌더 캡처로 확인했다. 버튼은 확인 화면을 연 뒤 기존 ReturnToSoloMenu 경로로 이동한다.
- 9캐릭터 × 전용무기/E/R 27개 애니메이션을 제작·임포트했다. 자체 제작 관절 키프레임이며 원작 모션 추출본이 아니다.
- Blender에서 27개 모두 관절 변화 확인. UE 런타임에서 loaded=1 playing=1 restored=1 priority=1 clips=27 확인 (AttackMotionAcceptance.log).
- 짧은 아케이드 팝 계열 사망음 3종을 생성해 Minion 사망 이벤트에 연결했다. 영상과의 청감 일치 여부는 미검증이다.
- AttackAudioPackaging.log: 빌드/패키징 성공, 로비와4맵 시작 통과.
- AttackAudioCoop.log: 로컬 방장+3클라이언트 접속 통과. 서로 다른 PC의 Steam 테스트는 별도 필요.
- AttackAudioShare.log: 69파일 1,451,830,809바이트 복사와 주요 파일 해시 대조 통과.

이전 공유본은 삭제하지 않았다. 전체 맵 디테일·원작 대비 모션 완성도·전 캐릭터 실전 완주는 아직 완료 판정하지 않는다.
