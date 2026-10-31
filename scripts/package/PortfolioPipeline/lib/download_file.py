import sys
import re
from pathlib import Path
from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.http import MediaIoBaseDownload

SCOPES = ['https://www.googleapis.com/auth/drive.readonly']


class GoogleDriveClient:
    def __init__(self):
        self.creds = self._get_credentials()
        self.service = build('drive', 'v3', credentials=self.creds)

    def _get_credentials(self):
        creds = None
        script_dir = Path(__file__).parent.resolve()
        credentials_path = script_dir / 'credentials.json'
        token_path = script_dir / 'token.json'

        if token_path.exists():
            creds = Credentials.from_authorized_user_file(str(token_path), SCOPES)
        if not creds or not creds.valid:
            if creds and creds.expired and creds.refresh_token:
                creds.refresh(Request())
            else:
                flow = InstalledAppFlow.from_client_secrets_file(str(credentials_path), SCOPES)
                creds = flow.run_local_server(port=0)
            token_path.write_text(creds.to_json(), encoding='utf-8')
        return creds

    def clean_filename(self, filename: str) -> str:
        return re.sub(r'[\\/*?:"<>|]', '_', filename)

    def download_file_from_drive(self, file_id: str, output_dir: str):
        metadata = self.service.files().get(
            fileId=file_id, fields="name, mimeType"
        ).execute()
        file_name = self.clean_filename(metadata.get('name', 'file'))
        mime_type = metadata.get('mimeType')

        output_dir_path = Path(output_dir)
        if mime_type == 'application/vnd.google-apps.presentation':
            output_path = output_dir_path / f"{file_name}.pdf"
            request = self.service.files().export_media(
                fileId=file_id, mimeType='application/pdf'
            )
        else:
            output_path = output_dir_path / file_name
            request = self.service.files().get_media(fileId=file_id)

        output_dir_path.mkdir(parents=True, exist_ok=True)
        with open(output_path, 'wb') as fh:
            downloader = MediaIoBaseDownload(fh, request)
            done = False
            while not done:
                status, done = downloader.next_chunk()
                if status:
                    print(f"ダウンロード進捗: {int(status.progress() * 100)}%")
        print(f"ダウンロード完了: {output_path}")


if __name__ == '__main__':
    if len(sys.argv) < 3:
        # Check authentication only if no arguments provided
        GoogleDriveClient()
        print("Google Drive API authentication check passed.")
        sys.exit(0)

    file_id = sys.argv[1]
    output_dir = sys.argv[2]
    try:
        client = GoogleDriveClient()
        client.download_file_from_drive(file_id, output_dir)
    except Exception as e:
        print(f"Error during Google Drive download: {e}", file=sys.stderr)
        sys.exit(1)
