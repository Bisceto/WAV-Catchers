import json
import base64
from os import environ
import logging
import boto3
from datetime import datetime

from botocore.exceptions import ClientError
logger = logging.getLogger(__name__)

# Get the boto3 client.
rek_client = boto3.client('rekognition')
iot_client = boto3.client('iot-data', region_name='ap-southeast-1')
s3 = boto3.client('s3')

bucket_name = 'airrehnsimages'
current_time = datetime.now().strftime("%Y%m%d%H%M%S")
file_name = f"image_{current_time}.jpg"


def lambda_handler(event, context):
    labels = []
    print("Received event:", event)
    try:

        # Determine image source.
        if 'data' in event:
            # Decode the image
            image_bytes = event['data']
            img_b64decoded = base64.b64decode(image_bytes)
            image = {'Bytes': img_b64decoded}
            s3.put_object(Bucket=bucket_name, Key=file_name, Body=img_b64decoded)
        else:
            raise ValueError(
                'Invalid source. Only image base 64 encoded image bytes or S3Object are supported.')


        # Analyze the image.
        response = rek_client.detect_labels(Image=image)

        # Get the labels
        for label in response['Labels']:
            if (label['Confidence'] > 70):
                labels.append(label['Name'])
        print(labels)
        
        # If contains a person
        if 'Person' in labels:
            print("there is a person")
            pub_response = iot_client.publish(topic = "PersonDetected", payload ="Hi")
            if(pub_response):
                print("Message successfully published!")
        

        lambda_response = {
            "statusCode": 200,
            "body": json.dumps(labels)
        }
        
    except ClientError as err:
        error_message = f"Couldn't analyze image. " + \
            err.response['Error']['Message']

        lambda_response = {
            'statusCode': 400,
            'body': {
                "Error": err.response['Error']['Code'],
                "ErrorMessage": error_message
            }
        }
        logger.error("Error function %s: %s",
            context.invoked_function_arn, error_message)

    except ValueError as val_error:
        lambda_response = {
            'statusCode': 400,
            'body': {
                "Error": "ValueError",
                "ErrorMessage": format(val_error)
            }
        }
        logger.error("Error function %s: %s",
            context.invoked_function_arn, format(val_error))
            
    
    return lambda_response