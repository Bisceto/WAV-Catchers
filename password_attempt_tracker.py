import httplib2
import os
import sys

from datetime import datetime
from time import *
from apiclient import discovery
from google.oauth2 import service_account

def add_password_attempt(): 
    scopes = ["https://www.googleapis.com/auth/drive", "https://www.googleapis.com/auth/drive.file", "https://www.googleapis.com/auth/spreadsheets"]
    secret_file = os.path.join(os.getcwd(), 'service_bot_credentials.json')

    spreadsheet_id = '1_ElKGJXLOFyxICCdZMTgvFqbVZvfdv0FLZt6K-dbgU8'
    entry_number_range = 'Sheet1!A1'

    credentials = service_account.Credentials.from_service_account_file(secret_file, scopes=scopes)
    service = discovery.build('sheets', 'v4', credentials=credentials)
    
    # Read current amount of entries
    response = service.spreadsheets().values().get(spreadsheetId=spreadsheet_id, range=entry_number_range).execute()
    entry_number = int(response.get('values', [])[0][0]) + 1
    new_timestamp_range = 'Sheet1!B' + str(entry_number)
    
    # Update entry amount
    values = [ [entry_number] ]
    data = { 'values' : values }
    service.spreadsheets().values().update(spreadsheetId=spreadsheet_id, body=data, range=entry_number_range, valueInputOption='USER_ENTERED').execute()
    
    # Write new timestamps
    values = [ [datetime.utcfromtimestamp(time()).strftime('%Y-%m-%d %H:%M:%S')] ]
    data = { 'values' : values }
    service.spreadsheets().values().update(spreadsheetId=spreadsheet_id, body=data, range=new_timestamp_range, valueInputOption='USER_ENTERED').execute()

def reset():
    scopes = ["https://www.googleapis.com/auth/drive", "https://www.googleapis.com/auth/drive.file", "https://www.googleapis.com/auth/spreadsheets"]
    secret_file = os.path.join(os.getcwd(), 'service_bot_credentials.json')

    spreadsheet_id = '1_ElKGJXLOFyxICCdZMTgvFqbVZvfdv0FLZt6K-dbgU8'
    entry_number_range = 'Sheet1!A1'

    credentials = service_account.Credentials.from_service_account_file(secret_file, scopes=scopes)
    service = discovery.build('sheets', 'v4', credentials=credentials)
    
    # Read current amount of entries
    response = service.spreadsheets().values().get(spreadsheetId=spreadsheet_id, range=entry_number_range).execute()
    entry_number = response.get('values', [])[0][0]
    
    if int(entry_number) <= 0: return
    
    timestamps_range = 'Sheet1!B1:B' + entry_number
    
    # Update entry amount
    values = [ [0] ]
    data = { 'values' : values }
    service.spreadsheets().values().update(spreadsheetId=spreadsheet_id, body=data, range=entry_number_range, valueInputOption='USER_ENTERED').execute()
    
    # Clear timestamps
    service.spreadsheets().values().clear(spreadsheetId=spreadsheet_id, range=timestamps_range).execute()